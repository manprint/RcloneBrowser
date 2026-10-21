# CI helper: rerun each test that ctest reported as failed, directly, so that a
# test that dies before QtTest flushes its output still shows its log and exit code.
param([string]$BuildDir = "build", [string]$Fixtures = "tests/fixtures")
$failed = Join-Path $BuildDir "Testing/Temporary/LastTestsFailed.log"
if (-not (Test-Path $failed)) { Write-Output "no LastTestsFailed.log"; exit 0 }
$env:QT_QPA_PLATFORM = "offscreen"
$env:RB_FIXTURES = (Resolve-Path $Fixtures).Path
foreach ($line in Get-Content $failed) {
  $name = ($line -split ':', 2)[1]
  $exe = Get-ChildItem -Recurse -Path $BuildDir -Filter "$name.exe" | Select-Object -First 1
  if (-not $exe) { Write-Output "${name}: executable not found"; continue }
  $log = Join-Path $env:RUNNER_TEMP "$name.txt"
  & $exe.FullName -o "$log,txt" -v2
  Write-Output ("{0}: exit code 0x{1:X8}" -f $name, $LASTEXITCODE)
  if (Test-Path $log) { Get-Content $log } else { Write-Output "${name}: no QtTest log written" }
}
