# Aura Project Collaboration Rules

## Project context and sources of truth

- Project root: `E:\UE5_Item\Aura_UE58`. Project file: `Aura.uproject`.
- Perforce client: `Aura_UE58_WS`. Engine: `E:\UE5_Download\UE_5.8` (UE 5.8.2, CL 56702186).
- Run project commands with the project root as the explicit working directory. Specify `Aura_UE58_WS` for Perforce operations; verify the client Root before any Perforce write.
- Keep `E:\UE5_Item\Aura` and `Aura_WS` read-only. Do not write, sync, or revert there, develop across the two projects, or copy UE 5.8 assets back to that checkout.
- Before an engineering task, read `Docs/Engineering/Current.md`, `Docs/Engineering/Handoff.md`, and the relevant task record. Check further facts as needed. Do not scan all documentation by default or rely on unrecorded assumptions from another conversation.

## Task ownership and delegation

- Follow the user's task-specific assignment of a design owner and an execution owner. The same AI may hold both roles. If the user gives an execution request without naming an owner, the receiving AI owns the task scope, delegation, review, evidence, and delivery. This does not create a permanent role assignment.
- Delegate a bounded subtask by default. Work directly when the task is simple or indivisible, or when a suitable subagent is unavailable; report which path was actually used.
- Multiple subagents may run concurrently when their tasks are independent and their write scopes do not overlap.
- DeepSeek Flash is the default Executor model, independent of provider. Before invoking it, verify the available runtime's actual model mapping. If it is unavailable, say so; do not silently substitute another model or claim a call succeeded.
- Set each subagent to the highest reasoning effort supported by its actual model and runtime. If that effort is unavailable, report the level actually used; do not claim a higher level.
- Give subagents only the objective, exact files/assets in scope and their read/write permissions, necessary facts, API and network impact, and acceptance criteria. A subagent may not expand its own authority or write scope.
- The execution owner resolves ordinary issues within scope. Escalate a material requirement or design change, substantial scope expansion, new permission, important GAS/authority/replication/prediction/lifecycle change, conflicting evidence, repeated failures with an untrusted root cause, or high-risk API, compatibility, or asset risk to the user or the specialist the user assigned.
- A role assignment does not transfer tool-specific privileges. Follow the stricter limits of the runtime and the delegated tool.

## Authorization and changes

- User requests such as "add," "modify," "fix," "implement," "execute," or "handle" authorize implementation within their stated scope. Do not ask for a second approval merely because a plan was shown.
- Honor explicit read-only, design-only, no-Build/PIE, and no-Perforce-change limits literally.
- Before editing, state the exact files/assets, why they need changing, and the relevant effects. Preserve existing user work and make the smallest necessary change.
- Without explicit authorization, do not delete files, rename many files, perform a broad refactor, operate Unreal Editor, or edit or save Blueprint or `.uasset` assets. Do not change a public API without supporting evidence.
- Ask for additional authorization only when the scope is unclear or expands, an action is destructive, asset permission is missing, a material safety risk exists, or a consequential choice between incompatible options remains.
- Do not modify `.git` or use reset/checkout operations that overwrite user changes.

## Filesystem, system, and privacy boundaries

- Default read/write scope is this project. Outside it, read-only inputs are limited to attachments explicitly referenced in the current request, runtime-required policy or skill resources, and source-code exceptions authorized for an exact path.
- Do not access unrelated user home, Desktop, Downloads, Documents, Pictures, Videos, Music, other projects, system directories, browser data, IDE configuration, global Git configuration, system environment variables, or the registry.
- Do not read or disclose passwords, cookies, SSH keys, API keys, tokens, certificates, private keys, or wallets. If a project file appears to contain a secret, report only its path and general type; do not print, copy, upload, or log the value.
- You may run project-related queries, project scripts, and authorized builds. Do not install global software, change system or global configuration, startup items or scheduled tasks, format a disk, delete files outside the project, or download and execute unknown scripts.
- Do not upload or copy local content to an external destination, or start a cloud sandbox or remote repository task, without explicit authorization for the specified data and purpose.
- Do not modify, format, or generate files in the engine installation. Build tools reading engine files automatically does not authorize manual browsing. Do not scan the whole Engine or user directory or put either on a broad allowlist.
- The engine/plugin-source read-only exception is limited to Codex Sol or Kiro when current API verification genuinely requires it and Codex Sol has authorized the exact path and scope. If Sol is not involved, the user must authorize that exact path and scope. Other identities do not inherit this exception.
- OpenCode Luna remains project-only and has no engine-source access. Delegated OpenCode and Kiro agents must not launch nested agents or invoke each other. Do not run Kiro with `--trust-all-tools`, `--cloud`, or `--repo`.

## Perforce and concurrent work

- Treat Perforce as the source of truth for version state. Before writing, inspect the target file's opened state and diff. Run a precise `p4 edit` for a tracked file when needed. If checkout fails, stop that write; never bypass Perforce by changing read-only attributes.
- Unless explicitly authorized, do not submit, revert, run a mutating reconcile, delete or move Perforce files, or create or modify changelists. Authorization for a task does not cover unrelated opened files.
- Scoped `p4 reconcile -n` is available only to Codex Sol or to a tool explicitly permitted to use it. Delegated OpenCode and Kiro agents must not run any reconcile command.
- Keep one Writer per production scope and one Build Owner globally. Confirm the prior holder has stopped before taking over.
- Parallelize read-only investigations or writes to strictly independent scopes only. Do not split an interface from its callers, or a tightly coupled header from its implementation, across simultaneous Writers.
- Serialize Perforce state changes and MCP calls. Do not overlap a Build with another UHT/UBT task. Recheck current file contents immediately before writing to avoid overwriting concurrent changes.
- After editing, inspect the exact diff and preserve the file's encoding and line endings. Avoid unrelated formatting changes.

## Investigation and Unreal implementation

- Locate the real symbols, references, and call chain before designing a change. Expand inspection gradually when evidence is insufficient; do not rescan the whole repository or assume a root cause.
- Report exact files, assets, and symbols. Separate confirmed facts, reasonable inferences, and unknowns. Recheck current code before choosing a write location or changing code affected by pending edits, APIs, authority, or lifecycle.
- Prefer native read/search tools; use `rg` for shell-based search. Read-only inspection must not rewrite, reformat, or transcode files.
- Reuse the existing architecture and assets. Preserve unrelated gameplay behavior.
- Follow Unreal C++ conventions. Prefer UE types over `std` containers. Forward-declare types in headers where practical, and include concrete types in `.cpp` files.
- For UObject pointers, account for `UPROPERTY`, GC, `TObjectPtr`, weak references, and lifetime. Use reflection macros correctly. Do not add expensive work to `Tick`.
- Preserve public Blueprint API compatibility by default. When changing a `UFUNCTION`, `UPROPERTY`, or delegate, explain the impact on nodes, connections, recompilation, and data compatibility.
- For GAS or multiplayer changes, identify the ASC owner and the responsibilities of the server, owning client, and other clients. Explain authority, replication/RPC, prediction, lifecycle, and local UI updates where relevant.
- Ordinary delegates do not cross the network. Grant abilities and change authoritative state on the server. Call `MarkAbilitySpecDirty` on the Spec actually held by the ASC.
- Do not "fix" synchronization by removing an authority check or replacing server validation with a UI event.

## MCP, verification, and review

- Official MCP endpoint: `http://127.0.0.1:8000/mcp`. Before calling it, verify the listening process, editor path, project identity, and tools actually available in the current session.
- If a required capability is missing, report it. Do not use a third-party bridge, engine patch, or global-configuration workaround without separate authorization.
- Limit asset operations to authorized paths. Verify tool success, node and pin readback, applicable compilation, save/reload, and runtime behavior separately. Strings found inside a `.uasset` do not prove its complete wiring.
- Choose validation according to risk. Distinguish static inspection, UHT/C++ Build, Blueprint Compile, PIE, listen-server/client, dedicated server, reconnect, and packaging evidence. A successful compile does not prove runtime or multiplayer behavior. Mark unobserved results as unverified.
- Executor self-check may suffice for low-risk work. For medium-risk work, review the focused diff, direct call chain, and validation output. For GAS/network, GC/async, public Blueprint API, save compatibility, crashes, asset or security risk, or an explicit user review request, review the critical path.
- Identify who reviewed the work and whether the review was independent. Do not present self-review as independent. If the original symptom was not reproduced and retested, do not claim the bug is fixed based only on compilation.
- Before using JEV, read `Docs/Engineering/JEV_WORKFLOW.md` and check its current status against `Docs/Engineering/Current.md`. Define a checkpoint before implementing a task that requires a Gate. A JEV result grants no permission, does not replace the execution owner's review, and does not expand the scope of external data transfer.

## Communication and handoff

- Respond in Simplified Chinese by default. Keep commands, APIs, identifiers, and paths exact. Answer simple questions directly; for technical explanations, give the conclusion first, followed by the necessary evidence and limits.
- Give concise start and completion updates for ordinary work. Report failures, blockers, authorization needs, asset operations, scope expansion, and important unverified results promptly. State the actual executor and validation performed.
- Record important state in `Docs/Engineering/Current.md`, `Docs/Engineering/Handoff.md`, or the relevant task record. A handoff should include the objective, facts, decisions, owner/Writer/Build Owner, modified scope, validation, risks, unknowns, and next step.
