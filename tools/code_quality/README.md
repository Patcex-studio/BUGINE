# Code Quality Assurance & Refactoring System

A comprehensive C++20 static analysis and automated refactoring tool designed for military simulation systems.

## Features

### Dead Code Detection
- Identifies unused functions, variables, classes, and includes
- Call graph analysis for unreachable code detection
- Template and macro usage analysis

### Duplicate Code Detection
- Token-based similarity analysis
- AST-based structural comparison
- Sliding window and suffix tree algorithms
- Automatic refactoring suggestions

### Bug Detection
- Memory bugs (leaks, use-after-free, buffer overflows)
- Logic bugs (null pointer dereference, division by zero)
- Concurrency bugs (race conditions, deadlocks)
- Performance bugs (inefficient loops, memory usage)

### Automated Refactoring
- Safe refactoring with backup and rollback
- Dependency-aware refactoring order
- Post-refactoring testing validation
- Version control integration

### Comprehensive Testing
- Unit, integration, and performance tests
- Code coverage analysis
- Memory leak detection
- Concurrency testing

## Building

```bash
# From the project root
mkdir build
cd build
cmake .. -DBUILD_TOOLS=ON -DBUILD_CODE_QUALITY=ON
make -j$(nproc)
```

## Usage

### Code Analysis
```bash
./tools/code_quality/code_quality_tool /path/to/project analyze
```

### Automatic Refactoring
```bash
./tools/code_quality/code_quality_tool /path/to/project refactor
```

### Test Execution
```bash
./tools/code_quality/code_quality_tool /path/to/project test
```

### Full Pipeline
```bash
./tools/code_quality/code_quality_tool /path/to/project full
```

## Architecture

The system consists of several key components:

- `DeadCodeDetector`: Analyzes for unused code elements
- `DuplicateCodeDetector`: Finds and suggests refactoring for duplicate code
- `BugDetector`: Performs static analysis for various bug types
- `CodeQualityAnalyzer`: Orchestrates the full analysis pipeline
- `CodeRefactorer`: Applies safe automated refactoring
- `TestExecutor`: Runs comprehensive test suites

## Quality Targets

- **Dead code detection**: 100% accuracy for unused functions/variables
- **Duplicate detection**: 95%+ accuracy for significant duplicates
- **Bug detection**: 90%+ accuracy for critical bugs
- **Refactoring safety**: 100% functional preservation
- **Analysis performance**: < 30 minutes for 1M+ LOC projects

## Safety Guarantees

- **Functional preservation**: All refactoring maintains behavior
- **Thread safety**: No introduction of concurrency issues
- **Memory safety**: No memory corruption introduced
- **Compatibility**: No breaking API changes

## Integration

### Build System Integration
- CMake, Make, Gradle support
- Compiler warning integration
- Incremental analysis

### Version Control Integration
- Git branch management
- Safe commits with testing
- Change tracking

### CI/CD Pipeline Integration
- Automated quality gates
- Pull request checks
- Dashboard integration

## Military Simulation Specific Features

### Safety-Critical Analysis
- MISRA C++ compliance checking
- Formal verification support
- Fault tolerance analysis

### Performance-Critical Analysis
- Real-time constraint validation
- Memory usage optimization
- CPU utilization analysis

### Security Analysis
- Vulnerability detection
- Input validation analysis
- Memory safety verification

## Testing and Validation

The system includes comprehensive testing:
- Unit tests for all components
- Integration tests for full pipeline
- Performance benchmarks
- Accuracy validation against known issues

## Dependencies

- LLVM/Clang 12+ (for AST analysis)
- CMake 3.20+
- C++20 compatible compiler
- Optional: Testing frameworks (Google Test, Catch2)

## Contributing

1. Follow the established code quality standards
2. Add comprehensive tests for new features
3. Update documentation
4. Run full quality analysis before submitting

## License

This system is part of the Historica Universalis military simulation project.