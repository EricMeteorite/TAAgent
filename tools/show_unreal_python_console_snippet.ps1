Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "common.ps1")

Set-TaAgentProcessEnvironment

$repoRoot = Get-TaAgentRepoRoot
$listenerPath = Join-Path $repoRoot "unreal\agent-harness\ue_cli_listener_full.py"
$ueCliTempDir = Join-Path (Get-TaAgentRuntimeRoot) "tmp\ue_cli"

$pythonBlock = @'
import os
import sys
import types
path = r"{0}"
os.environ["UE_CLI_TEMP_DIR"] = r"{1}"

module_name = "_taagent_ue_cli"
existing_module = sys.modules.get(module_name)
if existing_module and getattr(existing_module, "stop_ue_cli", None):
	try:
		existing_module.stop_ue_cli()
	except Exception:
		pass

module = sys.modules.get(module_name)
if module is None:
	module = types.ModuleType(module_name)
	sys.modules[module_name] = module

module.__file__ = path
module.__dict__["__file__"] = path
module.__dict__["__name__"] = module_name
module.__dict__["__builtins__"] = __builtins__

with open(path, "r", encoding="utf-8") as handle:
	code = handle.read()
exec(compile(code, path, "exec"), module.__dict__, module.__dict__)
module.start_ue_cli()
print("[TAAgent] UE CLI listener started")
'@ -f $listenerPath, $ueCliTempDir

$pythonBytes = [System.Text.Encoding]::UTF8.GetBytes($pythonBlock)
$pythonBase64 = [System.Convert]::ToBase64String($pythonBytes)
$snippet = 'import base64; exec(compile(base64.b64decode("{0}").decode("utf-8"), "<TAAgent-UE-CLI>", "exec"), globals())' -f $pythonBase64

Write-Output $snippet
