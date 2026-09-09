#!/usr/bin/env python3
"""
SHADERed FastMCP Server
Model Context Protocol interface for SHADERed (v1.5.6) projects and shader workflows.

Provides agent tools to:
- Inspect pipeline passes, input layouts, and bound UAV/texture resources
- Read, add, update, and remove shader variables (uniforms and system variables)
- Directly toggle pinned variables (for the interactive Pinned UI)
- Execute headless rendering (-r, -rt, -rw, -rh) for automated testing & verification
- Compile and convert HLSL/GLSL shaders using SHADERed's built-in compiler
"""

import os
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any, Dict, List, Optional, Union

from mcp.server.fastmcp import FastMCP

mcp = FastMCP(
    "shadered-mcp",
    instructions="SHADERed IDE automation, project manipulation, and headless shader testing"
)

SHADERED_BIN = "/mnt/train/SHADERed/bin/SHADERed"


def _parse_xml(project_path: str) -> tuple[ET.ElementTree, ET.Element]:
    path = Path(project_path).resolve()
    if not path.exists():
        raise FileNotFoundError(f"SHADERed project not found: {project_path}")
    tree = ET.parse(path)
    return tree, tree.getroot()


def _save_xml(tree: ET.ElementTree, project_path: str) -> None:
    path = Path(project_path).resolve()
    ET.indent(tree, space="\t", level=0)
    tree.write(path, encoding="utf-8", xml_declaration=True)


@mcp.tool()
def list_passes(project_path: str) -> List[Dict[str, Any]]:
    """
    List all pipeline passes (compute, raster shader, audio) in a SHADERed project.
    
    Args:
        project_path: Absolute or relative path to the .sprj project file.
    """
    _, root = _parse_xml(project_path)
    pipeline = root.find("pipeline")
    if pipeline is None:
        return []

    passes = []
    for pass_elem in pipeline.findall("pass"):
        pass_info: Dict[str, Any] = {
            "name": pass_elem.get("name"),
            "type": pass_elem.get("type"),
            "active": pass_elem.get("active") == "true",
            "shaders": [],
        }

        # Shaders
        for shader in pass_elem.findall("shader"):
            pass_info["shaders"].append({
                "type": shader.get("type"),
                "path": shader.get("path"),
                "entry": shader.get("entry", "main"),
            })

        # Compute workgroups
        groupsize = pass_elem.find("groupsize")
        if groupsize is not None:
            pass_info["groupsize"] = {
                "x": int(groupsize.get("x", "1")),
                "y": int(groupsize.get("y", "1")),
                "z": int(groupsize.get("z", "1")),
            }

        # Items (e.g. ScreenQuad)
        items = pass_elem.find("items")
        if items is not None:
            pass_info["items"] = [item.get("name") for item in items.findall("item")]

        passes.append(pass_info)

    return passes


@mcp.tool()
def get_variables(project_path: str, pass_name: Optional[str] = None) -> List[Dict[str, Any]]:
    """
    Get all variables declared across passes or in a specific pass, including their
    type, system binding (e.g., Time), default values, and pinned state.

    Args:
        project_path: Path to the .sprj project file.
        pass_name: Optional name of the pass to filter by.
    """
    _, root = _parse_xml(project_path)

    pinned_map = set()
    settings = root.find("settings")
    if settings is not None:
        for entry in settings.findall("entry"):
            if entry.get("type") == "pinned":
                pinned_map.add((entry.get("owner", ""), entry.get("name", "")))

    variables = []
    pipeline = root.find("pipeline")
    if pipeline is None:
        return []

    for pass_elem in pipeline.findall("pass"):
        p_name = pass_elem.get("name", "")
        if pass_name and p_name != pass_name:
            continue

        vars_elem = pass_elem.find("variables")
        if vars_elem is None:
            continue

        for var in vars_elem.findall("variable"):
            v_name = var.get("name", "")
            v_type = var.get("type", "float")
            v_system = var.get("system")

            values = []
            for row in var.findall("row"):
                row_vals = [v.text for v in row.findall("value")]
                if len(row_vals) == 1:
                    values.append(row_vals[0])
                elif len(row_vals) > 1:
                    values.append(row_vals)

            variables.append({
                "pass": p_name,
                "name": v_name,
                "type": v_type,
                "system": v_system if v_system else None,
                "values": values if values else None,
                "is_pinned": (p_name, v_name) in pinned_map,
            })

    return variables


@mcp.tool()
def set_variable(
    project_path: str,
    pass_name: str,
    var_name: str,
    var_type: str = "float",
    value: Optional[Union[float, int, List[float]]] = None,
    system: Optional[str] = None,
    pin: bool = False
) -> str:
    """
    Add or update a variable on a specific shader or compute pass, with optional
    value initialization and pinning to the interactive Pinned UI.

    Args:
        project_path: Path to the .sprj project file.
        pass_name: Name of the pass (e.g. 'RaymarchCompute').
        var_name: Name of the variable (e.g. 'glowIntensity').
        var_type: Variable type ('float', 'float2', 'float3', 'float4', 'int', 'bool', etc.).
        value: Scalar or list of scalar values for initialization.
        system: System variable name (e.g., 'Time', 'ViewportSize', 'Cursor', etc.), or None.
        pin: If True, automatically pins this variable so it appears as a slider in the Pinned panel.
    """
    tree, root = _parse_xml(project_path)
    pipeline = root.find("pipeline")
    if pipeline is None:
        raise ValueError("Project contains no <pipeline> section")

    target_pass = None
    for pass_elem in pipeline.findall("pass"):
        if pass_elem.get("name") == pass_name:
            target_pass = pass_elem
            break

    if target_pass is None:
        raise ValueError(f"Pass '{pass_name}' not found in project.")

    vars_elem = target_pass.find("variables")
    if vars_elem is None:
        vars_elem = ET.SubElement(target_pass, "variables")

    target_var = None
    for var in vars_elem.findall("variable"):
        if var.get("name") == var_name:
            target_var = var
            break

    if target_var is None:
        target_var = ET.SubElement(vars_elem, "variable", name=var_name, type=var_type)
    else:
        target_var.set("type", var_type)

    if system:
        target_var.set("system", system)
        for child in list(target_var):
            target_var.remove(child)
    else:
        if "system" in target_var.attrib:
            del target_var.attrib["system"]
        for child in list(target_var):
            target_var.remove(child)

        if value is not None:
            row = ET.SubElement(target_var, "row")
            if isinstance(value, (list, tuple)):
                for v in value:
                    val_elem = ET.SubElement(row, "value")
                    val_elem.text = str(v)
            else:
                val_elem = ET.SubElement(row, "value")
                val_elem.text = str(value)

    if pin:
        settings = root.find("settings")
        if settings is None:
            settings = ET.SubElement(root, "settings")

        already_pinned = False
        for entry in settings.findall("entry"):
            if entry.get("type") == "pinned" and entry.get("name") == var_name and entry.get("owner") == pass_name:
                already_pinned = True
                break

        if not already_pinned:
            ET.SubElement(settings, "entry", type="pinned", name=var_name, owner=pass_name)

    _save_xml(tree, project_path)
    return f"Variable '{var_name}' on pass '{pass_name}' configured successfully (pinned={pin})."


@mcp.tool()
def pin_variable(project_path: str, pass_name: str, var_name: str, pinned: bool = True) -> str:
    """
    Toggle pinning for a variable so it appears in SHADERed's 'Pinned' sliders dock.

    Args:
        project_path: Path to the .sprj project file.
        pass_name: Name of the pass that owns the variable.
        var_name: Name of the variable to pin/unpin.
        pinned: True to pin, False to unpin.
    """
    tree, root = _parse_xml(project_path)
    settings = root.find("settings")
    if settings is None:
        settings = ET.SubElement(root, "settings")

    existing_entry = None
    for entry in settings.findall("entry"):
        if entry.get("type") == "pinned" and entry.get("name") == var_name and entry.get("owner") == pass_name:
            existing_entry = entry
            break

    if pinned and existing_entry is None:
        ET.SubElement(settings, "entry", type="pinned", name=var_name, owner=pass_name)
        _save_xml(tree, project_path)
        return f"Pinned '{var_name}' (owner: {pass_name}) to SHADERed Pinned panel."
    elif not pinned and existing_entry is not None:
        settings.remove(existing_entry)
        _save_xml(tree, project_path)
        return f"Unpinned '{var_name}' (owner: {pass_name})."

    return f"Variable '{var_name}' pinned state was already {pinned}."


@mcp.tool()
def render_still(
    project_path: str,
    output_image: str,
    time: float = 0.0,
    frame: int = 0,
    width: int = 1024,
    height: int = 768
) -> Dict[str, Any]:
    """
    Headlessly render a frame from a SHADERed project to an image file.

    Args:
        project_path: Path to the .sprj project file.
        output_image: Destination image path (.png or .jpg).
        time: Time system variable in seconds (default: 0.0).
        frame: Frame index integer (default: 0).
        width: Rendered width in pixels (default: 1024).
        height: Rendered height in pixels (default: 768).
    """
    cmd = [
        SHADERED_BIN,
        "-r", output_image,
        "-rt", str(time),
        "-rf", str(frame),
        "-rw", str(width),
        "-rh", str(height),
        project_path,
    ]

    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        out_path = Path(output_image).resolve()
        if not out_path.exists():
            return {
                "success": False,
                "error": f"Render command completed with code {proc.returncode}, but output file was not created.",
                "stdout": proc.stdout,
                "stderr": proc.stderr,
            }

        return {
            "success": True,
            "output_path": str(out_path),
            "size_bytes": out_path.stat().st_size,
            "stdout": proc.stdout.strip(),
        }
    except subprocess.TimeoutExpired:
        return {"success": False, "error": "Render process timed out after 30 seconds."}
    except Exception as e:
        return {"success": False, "error": str(e)}


@mcp.tool()
def compile_shader(
    shader_path: str,
    stage: str = "comp",
    entry: str = "main",
    target: str = "spirv",
    output_path: Optional[str] = None
) -> Dict[str, Any]:
    """
    Compile an HLSL/GLSL shader using SHADERed's built-in compiler.

    Args:
        shader_path: Path to shader file.
        stage: Shader stage ('vert', 'geom', 'tesc', 'tese', 'frag', 'comp').
        entry: Entry point function name (e.g., 'main').
        target: Target format ('spirv' or 'glsl').
        output_path: Optional destination file for output bytecode/text.
    """
    cmd = [
        SHADERED_BIN,
        "-c", shader_path,
        "-cl", "hlsl" if shader_path.endswith((".hlsl", ".slang")) else "glsl",
        "-cs", stage,
        "-e", entry,
        "-t", target,
    ]
    if output_path:
        cmd.extend(["-o", output_path])

    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=15)
    return {
        "success": proc.returncode == 0,
        "returncode": proc.returncode,
        "stdout": proc.stdout.strip(),
        "stderr": proc.stderr.strip(),
    }


if __name__ == "__main__":
    mcp.run()
