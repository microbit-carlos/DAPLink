#!/usr/bin/env python3
"""
Automated build and test setup script for DAPLink VFS tests.
This script automates all the manual steps described in test/vfs/README.md
"""
import os
import sys
import random
import shutil
import subprocess
from pathlib import Path

import yaml
import intelhex as ih


DBG_OUTPUT = False

# Paths
THIS_SCRIPT_DIR = Path(__file__).parent
DAPLINK_ROOT = THIS_SCRIPT_DIR.parent.parent
VFS_TEST_DIR = THIS_SCRIPT_DIR
TEST_FILES_DIR = VFS_TEST_DIR / "files"


def print_header(msg):
    print("\n" + "=" * 80 + "\n" + msg + "\n" + "=" * 80)


def run_command(cmd, cwd=None, check=True):
    """Run a shell command and return the result. Exit on error if check=True."""
    if DBG_OUTPUT:
        print(f"Running: {cmd}")
    try:
        result = subprocess.run(cmd, cwd=cwd, check=check, text=True)
        return result
    except subprocess.CalledProcessError as e:
        print(f"Command failed with exit code {e.returncode}")
        sys.exit(e.returncode)
    except Exception as e:
        print(f"Unexpected error: {e}")
        sys.exit(1)

def get_config():
    """Load the VFS targets configuration from vfs_boards.yaml"""
    config_file = VFS_TEST_DIR / "vfs_boards.yaml"
    print(f"Loading VFS targets config from: {config_file.absolute()}")
    if not config_file.exists():
        print(f"Error: VFS targets config file not found: {config_file}")
        sys.exit(1)
    try:
        with open(config_file, 'r') as f:
            config = yaml.safe_load(f)
    except Exception as e:
        print(f"Error loading VFS targets config: {e}")
        sys.exit(1)
    if "vfs_boards" not in config:
        print("Error: 'vfs_boards' section missing in config file.")
        sys.exit(1)
    mandatory_keys = ["build_name", "if_name", "if_mcu", "extra_sources",
                      "if_flash_regions", "target_flash_regions"]
    for board in config["vfs_boards"]:
        if not all(key in board for key in mandatory_keys):
            print(f"Error: One of the mandatory keys is missing in board config: {board}")
            print(f"Mandatory keys: {mandatory_keys}")
            sys.exit(1)
        for test_file in board.get("if_test_files", []) + board.get("target_test_files", []):
            if not (VFS_TEST_DIR / test_file).exists():
                print(f"Error: Test file not found: {test_file}")
                print(f"Path should be relative to: {VFS_TEST_DIR.absolute()}")
                sys.exit(1)
        # This additional dict added to the config to define the test cases
        board["test_cases"] = {"bl": [], "if": []}
    return config


def is_universal_hex(hex_str):
    """Check if a string is Universal Hex.

    Check that the first record is an Extended Linear Address followed by a
    Block Start record, which is only present in Universal Hex and required
    in that order.
    Spec: https://tech.microbit.org/software/spec-universal-hex
    """
    hex_lines = hex_str.splitlines()
    if not hex_lines[0].startswith(":02000004"):
        return False
    if not hex_lines[1].startswith(":0400000A"):
        return False
    return True


def build_daplink_targets(daplink_targets):
    """Build the specified DAPLink projects using progen_compile.py"""
    print(f"Building projects: {daplink_targets}\n")
    #TODO: clean remove
    run_command(
        [sys.executable, "tools/progen_compile.py", "--parallel", "--clean"]
            + daplink_targets,
        cwd=DAPLINK_ROOT)


def copying_daplink_files(daplink_targets, folder_output):
    """Copy the built DAPLink files to the specified output folder.

    :param daplink_targets: List of DAPLink target names to copy files from.
    :param folder_output: Path instance to the output folder.
    """
    folder_output.mkdir(exist_ok=True)
    for target_name in daplink_targets:
        target_build_dir = DAPLINK_ROOT / "projectfiles" / "make_gcc_arm" / f"{target_name}" / "build"
        if not target_build_dir.exists():
            print(f"Error: Build directory not found: {target_build_dir}")
            exit(1)
        for pattern in ["*_bl_crc.bin", "*_if_crc.bin", "*_bl_crc.hex","*_if_crc.hex"]:
            for fw_file in target_build_dir.glob(pattern):
                dst = folder_output / fw_file.name
                shutil.copy2(fw_file, dst)
                print(f"Copied: {dst.name}")


def generate_uf2_file(ih_file, uf2_family_id=None):
    """
    Generate UF2 files from Intel Hex files, with optional UF2 family ID.

    :param ih_file: Path instance to the input Intel Hex file.
    :param uf2_family_id: Optional UF2 family ID as a hex string (e.g. "0xADA52840").
    :return: Path instance to the generated UF2 file.
    """
    uf2conv_path = DAPLINK_ROOT / "tools" / "uf2" / "uf2conv.py"
    id_str = f"{uf2_family_id:#x}" if uf2_family_id else "none"
    uf2_file = ih_file.with_name(ih_file.stem + f"_id_{id_str}.uf2")
    print(f"Generating {uf2_file.name} with Family ID {id_str}.")
    uf2_file.unlink(missing_ok=True)
    cmd = [sys.executable, str(uf2conv_path.absolute())]
    if uf2_family_id:
        cmd.extend(["-f", id_str])
    cmd.extend([
        "-o", str(uf2_file.absolute()),
        str(ih_file.absolute())
    ])
    run_command(cmd)
    return uf2_file


def generate_bin_file(ih_file):
    """
    Generate a binary file from an Intel Hex file.

    :param ih_file: Path instance to the input Intel Hex file.
    :return: Path instance to the generated binary file.
    """
    bin_file = ih_file.with_suffix(".bin")
    print(f"Generating: {bin_file.name}")
    bin_file.unlink(missing_ok=True)
    run_command([
        "arm-none-eabi-objcopy",
        "--gap-fill", "0xFF",
        "-Iihex",
        "-Obinary",
        str(ih_file.absolute()),
        str(bin_file.absolute())
    ])
    print(f"File {bin_file.absolute()} generated.")
    return bin_file


def generate_rand_intel_hex_file(regions, path):
    """
    Generate an Intel Hex file with the specified memory regions filled with
    random data.

    :param regions: List of dicts with 'start' and 'end' keys defining memory regions.
    :param path: Path to save the generated Intel Hex file.
    """
    ih_obj = ih.IntelHex()
    if not regions:
        print(f"No regions specified for Intel Hex generation for {path.name}.")
        return
    regions_msg = []
    for region in regions:
        ih_obj.puts(
            region["start"],
            random.randbytes(region["end"] - region["start"])
        )
        regions_msg.append(f"0x{region['start']:X} - 0x{region['end']:X}")
    ih_obj.write_hex_file(path)
    print(f"File {path.name} generated with regions: {'; '.join(regions_msg)}")


def generate_vfs_tests_headers(config):
    """Generate vfs_tests.h header file with the test files for each build."""
    header_template = "/* Auto-generated header file for VFS test */\n"
    header_template += "#pragma once\n\n"
    header_template += '#include "vfs_test_types.h"\n\n'
    header_path = VFS_TEST_DIR / "vfs_tests.h"
    with open(header_path, 'w') as f:
        f.write(header_template)
        for board in config["vfs_boards"]:
            f.write(f"#if defined(BUILD_NAME_{board['build_name'].upper()})\n")
            f.write("\t#if defined(DAPLINK_BL)\n")
            f.write("\t\tvfs_tests_t tests[] = {\n")
            for test_case in board["test_cases"]["bl"]:
                f.write("\t\t\t{\n")
                f.write(f'\t\t\t\t"{test_case["input"].absolute().relative_to(VFS_TEST_DIR)}",\n')
                f.write(f'\t\t\t\t"{test_case["ref"].absolute().relative_to(VFS_TEST_DIR)}",\n')
                f.write(f'\t\t\t\t{"true" if test_case["out_of_order"] else "false"},\n')
                f.write(f'\t\t\t\t{test_case["location"]}\n')
                f.write("\t\t\t},\n")
            f.write("\t\t};\n")
            f.write("\t#elif defined(DAPLINK_IF)\n")
            f.write("\t\tvfs_tests_t tests[] = {\n")
            for test_case in board["test_cases"]["if"]:
                f.write("\t\t\t{\n")
                f.write(f'\t\t\t\t"{test_case["input"].absolute().relative_to(VFS_TEST_DIR)}",\n')
                f.write(f'\t\t\t\t"{test_case["ref"].absolute().relative_to(VFS_TEST_DIR)}",\n')
                f.write(f'\t\t\t\t{"true" if test_case["out_of_order"] else "false"},\n')
                f.write(f'\t\t\t\t{test_case["location"]}\n')
                f.write("\t\t\t},\n")
            f.write("\t\t};\n")
            f.write("\t#endif\n")
            f.write("#endif\n\n")

            print(f"{board['build_name']} bootloader test files:")
            for test in board["test_cases"]["bl"]:
                print(f'\t- {test["input"].absolute().relative_to(VFS_TEST_DIR)}', end=' -> ')
                print(f'{test["ref"].absolute().relative_to(VFS_TEST_DIR)} (out_of_order={test["out_of_order"]})')
            print(f"{board['build_name']} interface test files:")
            for test in board["test_cases"]["if"]:
                print(f'\t- {test["input"].absolute().relative_to(VFS_TEST_DIR)}', end=' -> ')
                print(f'{test["ref"].absolute().relative_to(VFS_TEST_DIR)} (out_of_order={test["out_of_order"]})')


def build_vfs_tests(config, build_dir):
    """Build the VFS test executables for each target defined in vfs_boards.yaml"""

    if build_dir.exists():
        shutil.rmtree(build_dir)

    for board in config["vfs_boards"]:
        print(f"\nBuilding VFS test: vfs_test_{board['build_name']}_bl/if")
        # Extra includes and sources need to be relative to VFS_TEST_DIR
        extra_includes = ";".join(
            os.path.relpath((DAPLINK_ROOT / inc).absolute(), VFS_TEST_DIR)
            for inc in board.get("extra_includes", [])
        )
        extra_sources = ";".join(
            os.path.relpath((DAPLINK_ROOT / src).absolute(), VFS_TEST_DIR)
            for src in board.get("extra_sources", [])
        )
        run_command([
                "cmake", "-S", ".", "-B", "build",
                f"-DBUILD_NAME={board['build_name']}",
                f"-DINTERFACE_MCU={board['if_mcu']}",
                f"-DEXTRA_INCLUDE_DIRS={extra_includes}",
                f"-DEXTRA_SOURCES={extra_sources}",
            ],
            cwd=VFS_TEST_DIR
        )
        run_command(["make", "-C", "build"], cwd=VFS_TEST_DIR)


def main():
    print(f"DAPLink root: {DAPLINK_ROOT}")
    print(f"VFS test dir: {VFS_TEST_DIR}")

    print_header("Loading VFS test configuration YAML file")
    config = get_config()

    print_header("Building DAPLink firmwares")
    daplink_targets = [board["if_name"] for board in config["vfs_boards"]]
    build_daplink_targets(daplink_targets)

    print_header("Copying DAPLink test files, converting to UF2 and adding to test cases")
    TEST_FILES_DIR.mkdir(exist_ok=True)
    dl_files_dir = TEST_FILES_DIR / "daplink"
    copying_daplink_files(daplink_targets, dl_files_dir)
    for board in config["vfs_boards"]:
        if_hex = dl_files_dir / f"{board['if_name']}_crc.hex"
        if_bin = dl_files_dir / f"{board['if_name']}_crc.bin"
        if not if_hex.is_file() or not if_bin.is_file():
            print(f"Error: DAPLink files not found for {board['if_name']}: {if_hex} or {if_bin}")
            sys.exit(1)
        board["test_cases"]["bl"].extend([{
            "input": if_hex,
            "ref": if_bin,
            "out_of_order": False,
            "location": "INTF_FLASH_IF"
        }, {
            "input": if_bin,
            "ref": if_bin,
            "out_of_order": False,
            "location": "INTF_FLASH_IF"
        }])
        uf2_if_ids = board.get("uf2_if_ids", [])
        for if_id in uf2_if_ids + [None]:
            if_uf2 = generate_uf2_file(if_hex, if_id)
            board["test_cases"]["bl"].append({
                "input": if_uf2,
                "ref": if_bin,
                "out_of_order": True,
                "location": "INTF_FLASH_IF"
            })
        print("")

    print_header("Converting config test files into bin + UF2 & add to test cases")
    for board in config["vfs_boards"]:
        for test_files, test_cases, location, uf2_ids in (
            (board.get("if_test_files", []), board["test_cases"]["bl"], "INTF_FLASH_IF", board.get("uf2_if_id", None)),
            (board.get("target_test_files", []), board["test_cases"]["if"], "TARGET_FLASH", board.get("uf2_target_ids", None)),
        ):
            for f_string in test_files:
                print(f"Processing file {f_string} for board {board['build_name']}:")
                hex_file = Path(f_string)
                if not hex_file.is_file() or hex_file.suffix.lower() != ".hex":
                    print(f"Error: Invalid hex test file specified: {hex_file}")
                    sys.exit(1)
                # TODO: Check for sectors or blocks universal hex
                if is_universal_hex(hex_file.read_text()):
                    print("Skipping converting Universal Hex file.\n")
                    bin_file = hex_file.with_suffix(".bin")
                    if not bin_file.is_file():
                        print(f"Error: Corresponding bin file not found for Universal Hex: {bin_file}")
                        sys.exit(1)
                    test_cases.append({
                        "input": hex_file,
                        "ref": bin_file,
                        "out_of_order": True,
                        "location": location
                    })
                    continue
                bin_file = generate_bin_file(hex_file)
                test_cases.extend([{
                    "input": hex_file,
                    "ref": bin_file,
                    "out_of_order": False,
                    "location": location
                }, {
                    "input": bin_file,
                    "ref": bin_file,
                    "out_of_order": False,
                    "location": location
                }])
                uf2_ids_list = uf2_ids if isinstance(uf2_ids, list) else []
                for uf2_id in uf2_ids_list + [None]:
                    uf2_file = generate_uf2_file(hex_file, uf2_id)
                    test_cases.append({
                        "input": uf2_file,
                        "ref": bin_file,
                        "out_of_order": True,
                        "location": location
                    })
                print("")

    print_header("Generating Intel Hex & UF2 files for Bootloader & Interface testing")
    temp_files_dir = TEST_FILES_DIR / "tmp"
    temp_files_dir.mkdir(exist_ok=True)
    for board in config["vfs_boards"]:
        # Generate interface hex/uf2 files for bootloader testing
        ih_if_path = temp_files_dir / f"{board['build_name']}_if_random.hex"
        generate_rand_intel_hex_file(board.get("if_flash_regions", []), ih_if_path)
        bin_if_path = generate_bin_file(ih_if_path)
        board["test_cases"]["bl"].append({
            "input": ih_if_path,
            "ref": bin_if_path,
            "out_of_order": False,
            "location": "INTF_FLASH_IF"
        })
        interface_uf2_ids = board.get("uf2_if_ids", [])
        for if_id in interface_uf2_ids + [None]:
            uf2_if_path = generate_uf2_file(ih_if_path, if_id)
            board["test_cases"]["bl"].append({
                "input": uf2_if_path,
                "ref": bin_if_path,
                "out_of_order": True,
                "location": "INTF_FLASH_IF"
            })
        print("")
        # Generate target hex/uf2 files for interface testing
        ih_target_path = temp_files_dir / f"{board['build_name']}_target_random.hex"
        generate_rand_intel_hex_file(board.get("target_flash_regions", []), ih_target_path)
        bin_target_path = generate_bin_file(ih_target_path)
        board["test_cases"]["if"].append({
            "input": ih_target_path,
            "ref": bin_target_path,
            "out_of_order": False,
            "location": "TARGET_FLASH"
        })
        target_uf2_ids = board.get("uf2_target_ids", [])
        for target_id in target_uf2_ids + [None]:
            uf2_target_path = generate_uf2_file(ih_target_path, target_id)
            board["test_cases"]["if"].append({
                "input": uf2_target_path,
                "ref": bin_target_path,
                "out_of_order": True,
                "location": "TARGET_FLASH"
            })
        print("")

    print_header("Generating test header file")
    generate_vfs_tests_headers(config)

    print_header("Building test executables")
    build_dir = VFS_TEST_DIR / "build"
    build_vfs_tests(config, build_dir)

    print_header("Build complete!")
    print("Available VFS test executables:")
    for exe in build_dir.glob("vfs_test_*"):
        if exe.is_file() and os.access(exe, os.X_OK):
            print(f"  ./{exe.relative_to(VFS_TEST_DIR)}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
