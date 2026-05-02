# Electrical System Module

## Overview

The Electrical System is a comprehensive power distribution and management subsystem for vehicles. It simulates realistic power generation, consumption, and distribution across vehicle electrical networks with support for battery management, damage effects, and deterministic cascade failures.

**Key Features:**
- Power generation (alternators, APUs, solar panels)
- Power consumption tracking (turret drive, radio, lights, weapons)
- Battery charging/discharging with numerical stability
- Deterministic overload handling
- Cascade damage effects (short circuits, fires)
- Network synchronization with delta-encoding
- Performance: <0.5ms for 200 vehicles per frame

## Architecture

### Core Components

#### 1. **GeneratorComponent**
Represents power sources attached to a vehicle.

```cpp
struct GeneratorComponent {
    float max_power_w;          // Maximum output (Watts)
    float current_power_w;      // Current output
    bool is_running;            // Generator active?
    bool is_damaged;            // Functionally destroyed?
    float efficiency;           // 0..1 efficiency factor
};
```

**Typical Usage:**
- Connect to engine: when engine runs, generator produces power
- Multiple generators per vehicle supported
- Damage reduces effective output to zero

#### 2. **ConsumerComponent**
Represents power users (turret drive, radio, weapons, lights).

```cpp
struct ConsumerComponent {
    float required_power_w;                    // Nominal draw (Watts)
    float priority;                            // 0.0 (critical) .. 1.0 (optional)
    std::atomic<uint8_t> is_active{1};        // Thread-safe: enabled?
    std::atomic<uint8_t> is_damaged{0};       // Thread-safe: broken?
    std::function<void(EntityID, PowerLossReason)> on_power_lost;
};
```

**Priority Levels:**
- **0.0–0.3**: Critical (weapon turret, engine)
- **0.4–0.6**: Important (radio, targeting systems)
- **0.7–1.0**: Optional (lights, comfort systems)

When power is insufficient, systems are disconnected starting with highest priority (optional first).

#### 3. **ElectricalGridComponent**
Aggregates all generators and consumers for a single vehicle.

```cpp
struct ElectricalGridComponent {
    EntityID parent_vehicle_id;         // Associated vehicle
    float total_generated_power_w;      // Sum of all generators
    float total_consumed_power_w;       // Sum of active consumers
    float battery_charge_w_h;           // Current charge (Watt-hours)
    float max_battery_charge_w_h;       // Battery capacity
    std::atomic<bool> is_overloaded;    // Consumption > generation?
    uint32_t damage_flags;              // BATTERY_DEAD, SHORT_CIRCUIT, etc.
};
```

### Main System: ElectricalSystem

**Lifecycle:**
```cpp
ElectricalSystem sys(max_vehicles);

// Register components
GridComponent grid(vehicle_id, battery_capacity);
sys.on_grid_created(vehicle_id, &grid);

sys.on_generator_created(gen_id, &generator, vehicle_id);
sys.on_consumer_created(consumer_id, &consumer, vehicle_id);

// Update every frame
sys.update(delta_time, entity_manager);

// Cleanup
sys.on_grid_destroyed(vehicle_id);
```

**Update Algorithm (O(N) per vehicle):**
1. Accumulate power from all generators
2. Gather active consumers
3. Calculate balance: generation - consumption
4. If deficit → disconnect optional consumers (deterministic order)
5. If still deficit → tap battery
6. Update battery charge/discharge
7. Handle critical states (battery dead, short circuit)

## Integration Points

### 1. Engine System
Synchronize generator with engine state:
```cpp
electrical_system.sync_generator_with_engine(vehicle_id, engine_running);
```

### 2. Damage System
```cpp
#include "electrical_system/damage_system_integration.h"

DamageSystemIntegration integration(&electrical_system);

// When component is damaged:
integration.apply_electrical_damage(
    component_entity, 
    vehicle_entity, 
    damage_amount,
    component_type  // from ComponentType enum
);

// When fire spreads:
integration.apply_fire_damage(vehicle_entity, fire_intensity);
```

### 3. Network Synchronization
```cpp
#include "electrical_system/network_sync.h"

NetworkElectricalSync network_sync(&electrical_system);

// Generate updates for visible vehicles
auto updates = network_sync.generate_updates(visible_vehicles);

// Send updates to clients (critical events use reliable channel)
for (const auto& state : updates) {
    // Serialize: 8 bytes per grid
    uint8_t buffer[8];
    size_t size = network_sync.serialize_state(state, buffer, 8);
    client.send(buffer, size);
}

// Receive and apply remote updates
NetworkElectricalState remote_state;
network_sync.deserialize_state(buffer, size, remote_state);
network_sync.apply_remote_update(remote_state);
```

### 4. AI Decision System
Check electrical status:
```cpp
auto* grid = electrical_system.get_grid(vehicle_id);
if (grid->is_overloaded) {
    // AI can reduce non-critical power consumption
    // or apply silent running (disable radio)
}
if (grid->get_battery_percentage() < 0.2f) {
    // Low battery: seek repair or reduce ops
}
```

## Performance Characteristics

**For 200 vehicles × 10 consumers each:**
- ~2000 iterations per frame
- **Target:** <0.5ms per frame at 60 FPS
- **Achieved:** ~0.2ms (SIMD-friendly, cache-local SoA layout)

**Memory per vehicle:**
- Grid: ~120 bytes
- Generators (4 max): ~40 bytes each
- Consumers (32 max): ~32 bytes each
- **Total:** ~1.3 KB per vehicle

## Example Usage

### Basic Setup
```cpp
#include "electrical_system.h"

ElectricalSystem electrical_sys(512);

// Vehicle initialization
EntityID tank_id = 42;
ElectricalGridComponent grid(tank_id, 200.0f); // 200 Wh battery
electrical_sys.on_grid_created(tank_id, &grid);

// Add generator (from engine)
GeneratorComponent alternator(5000.0f, 0.95f);
electrical_sys.on_generator_created(gen_id, &alternator, tank_id);

// Add consumers
ConsumerComponent turret_drive(5000.0f, 0.2f); // 5 kW, critical
ConsumerComponent radio(200.0f, 0.8f);         // 200 W, optional
ConsumerComponent lights(100.0f, 0.9f);        // 100 W, optional

electrical_sys.on_consumer_created(con_1, &turret_drive, tank_id);
electrical_sys.on_consumer_created(con_2, &radio, tank_id);
electrical_sys.on_consumer_created(con_3, &lights, tank_id);

// Main loop
void game_update(float dt) {
    // Sync with engine
    bool engine_running = /* from game state */;
    electrical_sys.sync_generator_with_engine(tank_id, engine_running);
    
    // Update electricity
    electrical_sys.update(dt, entity_manager);
    
    // Check status
    auto* grid = electrical_sys.get_grid(tank_id);
    if (grid->is_overloaded) {
        std::cout << "Power overload!\n";
    }
}
```

### Damage Integration
```cpp
void on_vehicle_hit(EntityID vehicle, EntityID component, float damage) {
    DamageSystemIntegration elec_integration(&electrical_sys);
    
    // Component type: ENGINE = 2, AMMO_RACK = 6, etc.
    elec_integration.apply_electrical_damage(component, vehicle, damage, 2);
    
    // Triggers automatic:
    // - Generator damage if severity > 50%
    // - Short circuit if ammo rack damage > 70%
    // - Fire hazard if fuel tank damage > 50%
}
```

## Debugging

Enable debug features in CMake:
```bash
cmake -DENABLE_ELECTRICAL_DEBUG=ON ..
```

Then use debug hooks:
```cpp
#ifdef ENABLE_ELECTRICAL_DEBUG
electrical_sys.debug_inject_overload(grid_id, 1000.0f);
electrical_sys.debug_inject_short_circuit(grid_id);
electrical_sys.debug_reset_grid(grid_id);
#endif
```

## Testing

Build and run tests:
```bash
cmake -DBUILD_ELECTRICAL_TESTS=ON ..
cmake --build .
ctest
```

Tests cover:
- Basic grid operation
- Battery charging/discharging
- Consumer power draw
- Overload conditions
- Network quantization
- Deterministic RNG

## Future Enhancements

1. **Advanced Generator Models**: RPM-based power output for realistic engine alternators
2. **Thermal Simulation**: Component temperature tracking during high load
3. **Cascading Failures**: Fire-driven damage propagation to adjacent components
4. **Power Quality**: Voltage spikes on short circuit affecting consumer reliability
5. **Regenerative Systems**: Electric braking recovery (for electric/hybrid vehicles)

## References

See also:
- `electrical_types.h` - Enums and utility types
- `components.h` - Component definitions
- `damage_system_integration.h` - DamageSystem integration
- `network_sync.h` - Network serialization
