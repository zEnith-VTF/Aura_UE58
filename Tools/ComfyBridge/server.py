"""Project-scoped, dependency-free MCP bridge for a local ComfyUI server."""

from __future__ import annotations

import argparse
import json
import math
import re
import sys
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode, urlparse
from urllib.request import ProxyHandler, Request, build_opener


PROJECT_ROOT = Path(__file__).resolve().parents[2]
OUTPUT_ROOT = PROJECT_ROOT / "Saved" / "ComfyUI"
MAX_WORKFLOW_BYTES = 2 * 1024 * 1024
MAX_OUTPUT_BYTES = 100 * 1024 * 1024
PROTOCOL_VERSION = "2025-11-25"
OPENER = build_opener(ProxyHandler({}))  # Never send localhost requests to a proxy.


def local_url(value: str) -> str:
    parsed = urlparse(value)
    if (
        parsed.scheme != "http"
        or parsed.hostname not in {"127.0.0.1", "localhost", "::1"}
        or parsed.username
        or parsed.password
        or parsed.path not in {"", "/"}
        or parsed.query
        or parsed.fragment
    ):
        raise ValueError("ComfyUI URL must be a local HTTP origin, such as http://127.0.0.1:8188")
    return value.rstrip("/")


def request_json(base: str, route: str, payload: dict | None = None) -> dict | list:
    body = None if payload is None else json.dumps(payload, ensure_ascii=False).encode("utf-8")
    req = Request(base + route, data=body, headers={"Content-Type": "application/json"} if body else {})
    try:
        with OPENER.open(req, timeout=15) as response:
            return json.load(response)
    except HTTPError as exc:
        raise RuntimeError(f"ComfyUI returned HTTP {exc.code} for {route.split('?')[0]}") from None
    except (URLError, TimeoutError) as exc:
        raise RuntimeError(f"Cannot reach ComfyUI at {base}; start ComfyUI Desktop and check its port") from None


def project_workflow(name: str) -> dict:
    if not isinstance(name, str):
        raise ValueError("workflow_file must be a project-relative JSON path")
    path = (PROJECT_ROOT / name).resolve()
    if not path.is_relative_to(PROJECT_ROOT) or path.suffix.lower() != ".json" or not path.is_file():
        raise ValueError("workflow_file must name an existing JSON file inside the Aura project")
    if path.stat().st_size > MAX_WORKFLOW_BYTES:
        raise ValueError("Workflow JSON exceeds the 2 MB limit")
    workflow = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(workflow, dict) or not workflow or not all(
        isinstance(node, dict) and isinstance(node.get("class_type"), str)
        and isinstance(node.get("inputs"), dict) for node in workflow.values()
    ):
        raise ValueError("Expected a ComfyUI API-format workflow (File > Export Workflow (API))")
    return workflow


def prompt_id(value: str) -> str:
    if not isinstance(value, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,100}", value):
        raise ValueError("Invalid prompt_id")
    return value


def status(base: str, _: dict) -> dict:
    stats = request_json(base, "/system_stats")
    queue = request_json(base, "/queue")
    model_folders = {}
    for folder in ("checkpoints", "diffusion_models", "text_encoders", "vae"):
        names = request_json(base, "/models/" + folder)
        model_folders[folder] = {"count": len(names), "names": names[:20]} if isinstance(names, list) else {"count": None, "names": []}
    system = stats.get("system", {}) if isinstance(stats, dict) else {}
    return {
        "connected": True,
        "comfyui_version": system.get("comfyui_version"),
        "pending": len(queue.get("queue_pending", [])),
        "running": len(queue.get("queue_running", [])),
        "models": model_folders,
    }


def find_nodes(base: str, args: dict) -> dict:
    term = args.get("search", "")
    if not isinstance(term, str) or not term or len(term) > 80:
        raise ValueError("search must be 1-80 characters")
    info = request_json(base, "/object_info")
    matches = [name for name in info if term.casefold() in name.casefold()]
    return {"total": len(matches), "nodes": matches[:50]}


def submit_workflow(base: str, args: dict) -> dict:
    workflow = project_workflow(args.get("workflow_file", ""))
    overrides = args.get("overrides", [])
    if not isinstance(overrides, list) or len(overrides) > 20:
        raise ValueError("overrides must be a list of at most 20 input changes")
    for item in overrides:
        if not isinstance(item, dict):
            raise ValueError("Each override must be an object")
        node_id, input_name, value = item.get("node_id"), item.get("input"), item.get("value")
        if (
            not isinstance(node_id, str) or node_id not in workflow
            or not isinstance(input_name, str) or input_name not in workflow[node_id]["inputs"]
            or not isinstance(value, (str, int, float, bool)) or len(str(value)) > 16000
            or (isinstance(value, float) and not math.isfinite(value))
        ):
            raise ValueError("Override must target an existing node input with a short scalar value")
        workflow[node_id]["inputs"][input_name] = value
    response = request_json(base, "/prompt", {"prompt": workflow})
    if not isinstance(response, dict) or not response.get("prompt_id"):
        return {"submitted": False, "error": response.get("error") if isinstance(response, dict) else "Unknown error",
                "invalid_nodes": list(response.get("node_errors", {})) if isinstance(response, dict) else []}
    return {"submitted": True, "prompt_id": response["prompt_id"], "queue_number": response.get("number")}


def job_status(base: str, args: dict) -> dict:
    job_id = prompt_id(args.get("prompt_id", ""))
    history = request_json(base, "/history/" + job_id)
    entry = history.get(job_id) if isinstance(history, dict) else None
    if not entry:
        return {"prompt_id": job_id, "state": "pending_or_not_found"}
    outputs = {}
    for node_id, node_outputs in entry.get("outputs", {}).items():
        outputs[node_id] = {
            key: [{"filename": record.get("filename"), "subfolder": record.get("subfolder", ""),
                   "type": record.get("type", "output")}
                  for record in records if isinstance(record, dict) and record.get("filename")]
            for key, records in node_outputs.items() if isinstance(records, list)
        }
    return {"prompt_id": job_id, "state": entry.get("status", {}).get("status_str", "finished"),
            "outputs": outputs}


def save_output(base: str, args: dict) -> dict:
    job_id = prompt_id(args.get("prompt_id", ""))
    node_id = args.get("node_id")
    kind = args.get("kind", "images")
    index = args.get("index", 0)
    if not isinstance(node_id, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,100}", node_id):
        raise ValueError("Invalid node_id")
    if not isinstance(kind, str) or not re.fullmatch(r"[A-Za-z0-9_]{1,30}", kind):
        raise ValueError("Invalid output kind")
    if type(index) is not int or not 0 <= index < 100:
        raise ValueError("index must be an integer from 0 to 99")
    history = request_json(base, "/history/" + job_id)
    entry = history.get(job_id) if isinstance(history, dict) else None
    records = entry.get("outputs", {}).get(node_id, {}).get(kind, []) if entry else []
    if not isinstance(records, list) or index >= len(records):
        raise ValueError("No matching saved output; inspect comfy_job_status first")
    record = records[index]
    if not isinstance(record, dict) or not isinstance(record.get("filename"), str) or not record["filename"]:
        raise ValueError("Output has no downloadable filename")
    query = urlencode({"filename": record["filename"], "subfolder": record.get("subfolder", ""),
                       "type": record.get("type", "output")})
    try:
        with OPENER.open(Request(base + "/view?" + query), timeout=60) as response:
            if int(response.headers.get("Content-Length", "0")) > MAX_OUTPUT_BYTES:
                raise ValueError("Output exceeds the 100 MB bridge limit")
            data = response.read(MAX_OUTPUT_BYTES + 1)
    except HTTPError as exc:
        raise RuntimeError(f"ComfyUI returned HTTP {exc.code} while fetching output") from None
    except (URLError, TimeoutError):
        raise RuntimeError("ComfyUI became unavailable while fetching output") from None
    if len(data) > MAX_OUTPUT_BYTES:
        raise ValueError("Output exceeds the 100 MB bridge limit")
    suffix = Path(record["filename"]).suffix.lower()
    if not re.fullmatch(r"\.[a-z0-9]{1,8}", suffix):
        suffix = ".bin"
    target_dir = OUTPUT_ROOT / job_id
    target_dir.mkdir(parents=True, exist_ok=True)
    target = target_dir / f"{node_id}_{kind}_{index}{suffix}"
    if target.exists():
        raise ValueError("Output already exists in the project; it will not be overwritten")
    target.write_bytes(data)
    return {"saved": True, "path": str(target), "bytes": len(data)}


TOOLS = [
    {"name": "comfy_status", "description": "Check the local ComfyUI server, queue and installed model files. No generation.",
     "annotations": {"readOnlyHint": True},
     "inputSchema": {"type": "object", "properties": {}}},
    {"name": "comfy_find_nodes", "description": "Find node classes installed in local ComfyUI, such as 3D or video nodes.",
     "annotations": {"readOnlyHint": True},
     "inputSchema": {"type": "object", "properties": {"search": {"type": "string"}}, "required": ["search"]}},
    {"name": "comfy_submit_workflow", "description": "Submit an API-format JSON workflow inside the Aura project to local ComfyUI. Partner/API nodes may contact external services; use only with user authorization for their inputs.",
     "annotations": {"readOnlyHint": False},
     "inputSchema": {"type": "object", "properties": {"workflow_file": {"type": "string", "description": "Path relative to the Aura project root"},
         "overrides": {"type": "array", "items": {"type": "object", "properties": {"node_id": {"type": "string"}, "input": {"type": "string"}, "value": {}},
             "required": ["node_id", "input", "value"]}}}, "required": ["workflow_file"]}},
    {"name": "comfy_job_status", "description": "Check one ComfyUI prompt ID and list its generated outputs.",
     "annotations": {"readOnlyHint": True},
     "inputSchema": {"type": "object", "properties": {"prompt_id": {"type": "string"}}, "required": ["prompt_id"]}},
    {"name": "comfy_save_output", "description": "Copy one output from a completed local ComfyUI job into Aura/Saved/ComfyUI (100 MB limit).",
     "annotations": {"readOnlyHint": False},
     "inputSchema": {"type": "object", "properties": {"prompt_id": {"type": "string"}, "node_id": {"type": "string"},
         "kind": {"type": "string", "default": "images"}, "index": {"type": "integer", "default": 0}},
         "required": ["prompt_id", "node_id"]}},
]
HANDLERS = {"comfy_status": status, "comfy_find_nodes": find_nodes,
            "comfy_submit_workflow": submit_workflow, "comfy_job_status": job_status,
            "comfy_save_output": save_output}


def handle(message: dict, base: str) -> dict | None:
    if "id" not in message:
        return None
    method = message.get("method")
    if method == "initialize":
        requested = message.get("params", {}).get("protocolVersion")
        version = requested if requested in {"2024-11-05", "2025-03-26", "2025-06-18", PROTOCOL_VERSION} else PROTOCOL_VERSION
        result = {"protocolVersion": version, "capabilities": {"tools": {}},
                  "serverInfo": {"name": "aura-comfyui", "version": "0.1.0"},
                  "instructions": "Only use project-local workflows and localhost ComfyUI. Partner/API nodes can send data outside this computer; request explicit authorization before submitting such workflows."}
    elif method == "ping":
        result = {}
    elif method == "tools/list":
        result = {"tools": TOOLS}
    elif method == "tools/call":
        params = message.get("params", {})
        tool_name = params.get("name")
        if tool_name not in HANDLERS:
            result = {"content": [{"type": "text", "text": "Unknown tool"}], "isError": True}
        else:
            try:
                value = HANDLERS[tool_name](base, params.get("arguments") or {})
                result = {"content": [{"type": "text", "text": json.dumps(value, ensure_ascii=False)}]}
            except (ValueError, RuntimeError, OSError, json.JSONDecodeError) as exc:
                result = {"content": [{"type": "text", "text": str(exc)}], "isError": True}
    else:
        return {"jsonrpc": "2.0", "id": message["id"], "error": {"code": -32601, "message": "Method not found"}}
    return {"jsonrpc": "2.0", "id": message["id"], "result": result}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--url", default="http://127.0.0.1:8188")
    base = local_url(parser.parse_args().url)
    for line in sys.stdin:
        try:
            message = json.loads(line)
            if not isinstance(message, dict):
                continue
            response = handle(message, base)
            if response is not None:
                sys.stdout.write(json.dumps(response, ensure_ascii=False, separators=(",", ":")) + "\n")
                sys.stdout.flush()
        except json.JSONDecodeError:
            continue


if __name__ == "__main__":
    main()
