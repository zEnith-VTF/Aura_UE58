# Aura 项目的 ComfyUI Desktop 桥接

本目录的 `server.py` 是本地 MCP 服务。Codex 通过它访问本机 ComfyUI 的 HTTP API；它只使用 Python 标准库，不需要安装 ComfyUI 插件。桥接服务不会提供模型，也不会自动安装模型或启动 ComfyUI Desktop。

如果当前只想制作特效图片或贴图概念图，可以直接在 Codex 对话里描述画面并使用内置生图功能，无需先选 ComfyUI 的本地模型或线上 API 节点。生成的图片可以作为工作流参考或素材，再决定是否需要 ComfyUI 的节点控制。Codex 内置生图不会自动变成 ComfyUI 的生成节点。

## 第一次使用

1. 启动 ComfyUI Desktop，确认它的界面能打开。项目配置默认连接 `http://127.0.0.1:8188`。如果 Desktop 显示其他端口，修改项目 `.codex/config.toml` 中 `aura-comfyui` 的 `--url` 参数。
2. 在 Codex 的 MCP 服务器列表中确认 `aura-comfyui` 已加载；项目配置变更后需要重新加载该项目或重启 Codex。然后请 Codex 调用 `comfy_status`。
3. 在 ComfyUI 里先用模板熟悉工作流：载入模板，检查缺失节点与模型，填写提示词，点击 **Run**。没有本地模型时，依赖本地 checkpoint 的工作流无法生成；如选用 Partner/API 节点，先确认其账号、费用和数据传输条件。
4. 工作流在界面中可运行后，使用 **File → Export Workflow (API)** 导出 JSON，保存到此项目中，例如 `Tools/ComfyBridge/workflow_api.json`。普通的 **Save** JSON 不能直接提交给 `/prompt`。
5. 请 Codex 调用 `comfy_submit_workflow`，传入项目相对路径；返回的 `prompt_id` 可交给 `comfy_job_status` 查询。完成后用 `comfy_save_output` 将指定输出保存到项目的 `Saved/ComfyUI/<prompt_id>/`。

## 工具和边界

- `comfy_status`：查看连接状态、队列及 `checkpoints`、`diffusion_models`、`text_encoders`、`vae` 四类本地模型文件。
- `comfy_find_nodes`：搜索 Desktop 当前加载的节点类，例如 `3D`、`video`。
- `comfy_submit_workflow`：提交项目内的 API 格式 JSON；可用 `overrides` 指定节点输入，例如 `[{"node_id":"6","input":"text","value":"stone portal effect"}]`。
- `comfy_job_status`：查询工作流状态及输出节点的文件信息。
- `comfy_save_output`：将一个输出复制到项目 `Saved/ComfyUI`；单个文件上限 100 MB。

桥接只接受项目内的工作流文件，只连接本机 HTTP 地址。工作流中的 Partner/API 节点仍可能访问外部服务；提交这类工作流或含有项目素材的工作流前，先确认你允许传输的内容与用途。项目内无模型时，桥接连通本身不代表能生成特效、3D 模型或动画。
