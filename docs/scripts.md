## Project Scripts Overview

All scripts (except `run.sh`) are located in the `/scripts` directory.

- **run.sh**
	- Located in the project root.
	- Runs the entire build and run pipeline: builds, packages, and launches QEMU.
	- Accepts `-d` for debug mode (starts QEMU with GDB support).

- **build.sh**
	- Builds the project and produces the `kernel.elf` file.
	- Handles compilation and linking steps.

- **pack.sh**
	- Packages the `kernel.elf` into a bootable `.iso` image for QEMU.
	- Also performs linking if needed.

- **run_qemu.sh**
	- Launches QEMU to run the OS from the `.iso` image.
	- Accepts `-d` for debugging (GDB integration).

- **clean.sh**
    - Cleans up build artifacts and temporary files.

### Additional Notes
- Scripts are designed to be modular; you can run each step individually or use `run.sh` for the full pipeline.
- Ensure you have the required cross-compiler and QEMU installed.
- Output files are placed in the `build/` directory by default.