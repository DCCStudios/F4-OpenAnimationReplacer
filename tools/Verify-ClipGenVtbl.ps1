# Verify hkbClipGenerator vtable layout against an on-disk exe.
# Dumps slots 0..30 of hkbClipGenerator + hkbBehaviorGraph vtables,
# marks shared (inherited) slots, and scans each per-class override's
# body for a RIP-relative reference to the "Invalid clip generator"
# string (the definitive marker for hkbClipGenerator::activate).
param(
    [Parameter(Mandatory)] [string]$ExePath,
    [Parameter(Mandatory)] [uint64]$ClipVtblRva,   # REL::ID(1360555)
    [Parameter(Mandatory)] [uint64]$GraphVtblRva   # REL::ID(476513)
)

$bytes = [System.IO.File]::ReadAllBytes($ExePath)
Write-Host ("File: {0}  ({1} bytes)" -f $ExePath, $bytes.Length)

# --- PE section table -> RVA-to-file-offset mapping ---
$peOff = [BitConverter]::ToInt32($bytes, 0x3C)
$numSections = [BitConverter]::ToUInt16($bytes, $peOff + 6)
$optSize = [BitConverter]::ToUInt16($bytes, $peOff + 20)
$secBase = $peOff + 24 + $optSize
$imageBase = [BitConverter]::ToUInt64($bytes, $peOff + 24 + 24)
Write-Host ("ImageBase: 0x{0:X}" -f $imageBase)

$sections = @()
for ($i = 0; $i -lt $numSections; $i++) {
    $o = $secBase + $i * 40
    $name = [System.Text.Encoding]::ASCII.GetString($bytes, $o, 8).TrimEnd([char]0)
    $sections += [pscustomobject]@{
        Name = $name
        VirtSize = [BitConverter]::ToUInt32($bytes, $o + 8)
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

# --- locate the "Invalid clip generator" string ---
$needle = [System.Text.Encoding]::ASCII.GetBytes("Invalid clip generator")
$strFileOff = -1
for ($i = 0; $i -le $bytes.Length - $needle.Length; $i++) {
    if ($bytes[$i] -eq $needle[0]) {
        $match = $true
        for ($j = 1; $j -lt $needle.Length; $j++) {
            if ($bytes[$i + $j] -ne $needle[$j]) { $match = $false; break }
        }
        if ($match) { $strFileOff = $i; break }
    }
}
$strRva = [uint64]0
if ($strFileOff -ge 0) {
    foreach ($s in $sections) {
        if ($strFileOff -ge $s.RawPtr -and $strFileOff -lt ($s.RawPtr + $s.RawSize)) {
            $strRva = [uint64]($s.VirtAddr + ($strFileOff - $s.RawPtr))
        }
    }
    $ctx = [System.Text.Encoding]::ASCII.GetString($bytes, $strFileOff, 60).Split([char]0)[0]
    Write-Host ("String found: fileOff=0x{0:X} rva=0x{1:X}  '{2}'" -f $strFileOff, $strRva, $ctx)
} else {
    Write-Host "String NOT found!"
}

# Scan a function body for any RIP-relative LEA/MOV operand resolving to targetRva.
# Covers 48/4C 8D xx (LEA r64) with ModRM mod=00 rm=101 (RIP-relative).
function BodyReferencesRva([uint64]$funcRva, [uint64]$targetRva, [int]$scanLen = 0x600) {
    $off = RvaToOff $funcRva
    if ($off -lt 0) { return $false }
    $end = [Math]::Min($off + $scanLen, $bytes.Length - 8)
    for ($i = $off; $i -lt $end; $i++) {
        $b = $bytes[$i]
        if (($b -eq 0x48 -or $b -eq 0x4C) -and $bytes[$i+1] -eq 0x8D) {
            $modrm = $bytes[$i+2]
            if (($modrm -band 0xC7) -eq 0x05) {
                $disp = [BitConverter]::ToInt32($bytes, $i + 3)
                $instrEndRva = $funcRva + ($i - $off) + 7
                if (($instrEndRva + $disp) -eq $targetRva) { return $true }
            }
        }
    }
    return $false
}

# --- dump both vtables ---
$clipOff = RvaToOff $ClipVtblRva
$graphOff = RvaToOff $GraphVtblRva
Write-Host ""
Write-Host ("{0,-5} {1,-14} {2,-14} {3,-9} {4}" -f "slot", "clipTarget", "graphTarget", "shared?", "refsInvalidClipStr")
for ($slot = 0; $slot -le 30; $slot++) {
    $cva = [BitConverter]::ToUInt64($bytes, $clipOff + $slot * 8)
    $gva = [BitConverter]::ToUInt64($bytes, $graphOff + $slot * 8)
    $crva = $cva - $imageBase
    $shared = if ($cva -eq $gva) { "SHARED" } else { "override" }
    $refs = ""
    if ($strRva -ne 0 -and $cva -gt $imageBase) {
        if (BodyReferencesRva $crva $strRva) { $refs = "<== ACTIVATE (refs string)" }
    }
    Write-Host ("{0,-5} 0x{1:X10}   0x{2:X10}   {3,-9} {4}" -f $slot, $cva, $gva, $shared, $refs)
}
