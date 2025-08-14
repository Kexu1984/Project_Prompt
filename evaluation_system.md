# Code Quality Evaluation System

This is a comprehensive code quality evaluation system designed to assess multiple code implementations within a specified directory and determine which implementation best meets the predefined functional requirements and code quality standards.

## Getting Started

Please carefully read `Project_intro.md` first to obtain complete information about this evaluation project and understand the overall scope and objectives.

## Code Components Under Evaluation

The code to be evaluated consists of two main components:

### 1. Model Code
- **Location**: `{device_name_v1}/output/python_{device_name.py}.py and {device_name_v2}/output/python_{device_name.py}`
- **Description**: Device model code generated based on the specifications in `{device_name_v1}/input/{device_name}_app_note.md` and the corresponding register map
- **Purpose**: Provides high-level device abstraction and modeling functionality

### 2. Driver Code  
- **Location**: `{device_name_v1}/output/driver/*.c/h and {device_name_v2}/output/driver/*.c/h`
- **Description**: Device driver code generated based on the specifications in `{device_name_v1}/input/{device_name}_app_note.md`` and the corresponding register map
- **Purpose**: Implements low-level hardware interaction and control mechanisms


## Evaluation Criteria

The evaluation focuses on verifying that the implementation aligns with the application note and register map specifications, while also assessing code quality aspects including readability and maintainability.

### 1. Functionality (Critical)
The code must implement all functions defined in the application note and correctly interact with registers specified in the register map. **If functionality is incorrect, the code implementation is automatically disqualified.**

Key aspects:
- Complete implementation of all required features
- Correct register-level interactions, all registers operation are implemented
- Correct interrupt logic and status
- Compliance with hardware specifications
- Proper error handling and edge cases

### 2. Readability
The code should be easy to read and understand, following consistent coding style and naming conventions. Appropriate comments and documentation enhance readability.

Evaluation points:
- Consistent coding style and formatting
- Clear and meaningful variable/function names
- Appropriate code comments and documentation
- Logical code organization and structure

### 3. Maintainability
The code should be easy to maintain and extend, following modular design principles and avoiding code duplication. Good code structure and clear interfaces improve maintainability.

Assessment criteria:
- Modular design and clear separation of concerns
- Minimal code duplication (DRY principle)
- Clear and well-defined interfaces
- Extensible architecture
- Proper abstraction levels

## Evaluation Output

After completing the evaluation across all dimensions, generate an evaluation report named `evaluation_report_{device_name}.md` and upload it to the `report/` directory. The report should include:

- **Scores for each evaluation dimension**
- **Detailed recommendations for improvement**
- **Overall assessment and ranking** (if multiple implementations are compared)
- **Critical issues and blockers** (if any)

**Note**: Only generate the evaluation report - no additional code should be produced during the evaluation process.
