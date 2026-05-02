-- ui_demo.lua

function render_ui_demo()
    ui.BeginWindow("Lua Panel")
        ui.Text("Hello from Lua!")
        if ui.Button("Spawn T-34") then
            -- send command
            -- assuming core.command is available
            core.command("spawn", {type="t34", x=0, y=0, z=0})
        end
        if ui.Button("Close") then
            -- maybe close window
        end
    ui.EndWindow()
end

-- Subscribe to event
ui.On("UnitDestroyed", function(unitId)
    print("Unit destroyed: " .. tostring(unitId))
end)