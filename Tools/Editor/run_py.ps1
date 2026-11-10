# Runs a Python script in the Museum editor commandlet (no window, null RHI). The editor must be CLOSED.
# Usage: powershell -File Tools\Editor\run_py.ps1 <script.py> [extra UE args]
# The log is copied to Saved\Logs\py_<script>.log; lines the scripts print start with "[Museum".
param([Parameter(Mandatory = $true)][string]$Script, [Parameter(ValueFromRemainingArguments = $true)][string[]]$Extra)

$Root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$Project = Join-Path $Root 'Museum.uproject'
if (-not $env:UE_ROOT) { throw 'Set UE_ROOT to the Unreal Engine 5.8 install folder' }
$Editor = Join-Path $env:UE_ROOT 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$Source = Resolve-Path $Script
$Log = Join-Path $Root ("Saved\Logs\py_" + [IO.Path]::GetFileNameWithoutExtension($Source) + ".log")
New-Item -ItemType Directory -Force (Split-Path $Log) | Out-Null
# -script= stops at the first space and this project's path has spaces, so the script runs from a copy in TEMP
$Stage = Join-Path $env:TEMP 'museum_py'
New-Item -ItemType Directory -Force $Stage | Out-Null
Copy-Item (Join-Path (Split-Path $Source) '*.py') $Stage -Force
$ScriptPath = Join-Path $Stage (Split-Path $Source -Leaf)

& $Editor $Project -run=pythonscript "-script=$ScriptPath" -unattended -nullrhi -nosplash -stdout -FullStdOutLogOutput -EnablePlugins=PythonScriptPlugin "-abslog=$Log" @Extra | Out-Null
$Code = $LASTEXITCODE
Select-String -Path $Log -Pattern '\[Museum|Error:|Traceback|Exception' | ForEach-Object { $_.Line }
"exit $Code, log $Log"
