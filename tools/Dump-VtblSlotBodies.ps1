# Hex-dump the first N bytes of selected vtable slot targets for manual x64 inspection.
param(
    [Parameter(Mandatory)] [string]$ExePath,
    [Parameter(Mandatory)] [uint64]$VtblRva,
    [Parameter(Mandatory)] [int[]]$Slots,
    [int]$DumpLen = 96
)

$bytes = [System.IO.File]::ReadAllBytes($ExePath)
$peOff = [BitConverter]::ToInt32($bytes, 0x3C)
$numSections = [BitConverter]::ToUInt16($bytes, $peOff + 6)
$optSize = [BitConverter]::ToUInt16($bytes, $peOff + 20)
$secBase = $peOff + 24 + $optSize
$imageBase = [BitConverter]::ToUInt64($bytes, $peOff + 24 + 24)

$sections = @()
for ($i = 0; $i -lt $numSections; $i++) {
    $o = $secBase + $i * 40
    $sections += [pscustomobject]@{
        VirtAddr = [BitConverter]::ToUInt32($bytes, $o + 12)
        RawSize  = [BitConverter]::ToUInt32($bytes, $o + 16)
        RawPtr   = [BitConverter]::ToUInt32($bytes, $o + 20)
    }
}
function RvaToOff([uint64]$rva) {
    foreach ($s in $script:sections) {
        if ($rva -ge $s.VirtAddr -and $rva -lt ($s.VirtAddr + $s.RawSize)) {
            return $s.RawPtr + ($rva - $s.VirtAddr)
        }
    }
    return -1
}

$vtblOff = RvaToOff $VtblRva
foreach ($slot in $Slots) {
    $va = [BitConverter]::ToUInt64($bytes, $vtblOff + $slot * 8)
    $rva = $va - $imageBase
    $off = RvaToOff $rva
    Write-Host ("--- slot {0}  VA=0x{1:X}  RVA=0x{2:X} ---" -f $slot, $va, $rva)
    for ($row = 0; $row -lt $DumpLen; $row += 16) {
        $hex = ($bytes[($off+$row)..($off+$row+15)] | ForEach-Object { $_.ToString("X2") }) -join " "
        Write-Host ("  +{0:X3}: {1}" -f $row, $hex)
    }
}
