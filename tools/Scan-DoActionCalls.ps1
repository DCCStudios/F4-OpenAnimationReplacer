# Scans a Fallout4.exe for CALL (E8) instructions inside the PlayerControls::DoAction
# function body and reports each call's displacement and target RVA. Used to locate
# the fire-empty auto-reload call site across runtimes (OG anchored it at +0x40A).
param(
    [Parameter(Mandatory)] [string]$ExePath,
    [Parameter(Mandatory)] [string]$FuncRvaHex,   # e.g. 0xF42FE0
    [int]$ScanLen = 0x800
)

$funcRva = [Convert]::ToUInt32($FuncRvaHex, 16)
$bytes = [System.IO.File]::ReadAllBytes($ExePath)

# PE header walk: map RVA to raw file offset via section table
$lfanew = [BitConverter]::ToInt32($bytes, 0x3C)
$numSections = [BitConverter]::ToUInt16($bytes, $lfanew + 6)
$optSize = [BitConverter]::ToUInt16($bytes, $lfanew + 20)
$secBase = $lfanew + 24 + $optSize

$off = -1
for ($i = 0; $i -lt $numSections; $i++) {
    $s = $secBase + 40 * $i
    $va = [BitConverter]::ToUInt32($bytes, $s + 12)
    $vsz = [BitConverter]::ToUInt32($bytes, $s + 8)
    $raw = [BitConverter]::ToUInt32($bytes, $s + 20)
    if ($funcRva -ge $va -and $funcRva -lt ($va + $vsz)) {
        $off = $raw + ($funcRva - $va)
        break
    }
}
if ($off -lt 0) { Write-Error "RVA not in any section"; exit 1 }

$ver = (Get-Item $ExePath).VersionInfo.FileVersion
Write-Output ("exe: {0} (version {1})" -f $ExePath, $ver)
Write-Output ("func RVA 0x{0:X} -> raw offset 0x{1:X}" -f $funcRva, $off)
Write-Output ("byte at +0x40A: 0x{0:X2}" -f $bytes[$off + 0x40A])

for ($i = 0; $i -lt $ScanLen; $i++) {
    if ($bytes[$off + $i] -eq 0xE8) {
        $rel = [BitConverter]::ToInt32($bytes, $off + $i + 1)
        $t = [int64]$funcRva + $i + 5 + $rel
        # Only report plausible in-image targets to skip false-positive E8 bytes
        if ($t -gt 0x1000 -and $t -lt $bytes.Length + 0x1000000) {
            $self = if ($t -eq $funcRva) { "  <== SELF-CALL" } else { "" }
            Write-Output ("  +0x{0:X3} CALL -> 0x{1:X}{2}" -f $i, $t, $self)
        }
    }
}
