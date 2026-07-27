# Check-ALIDs.ps1 - verify Address Library ID membership across runtime databases.
#
# The FO4 address library .bin format is a flat, ID-sorted array:
#   uint64 count, then count x { uint64 id, uint64 rva }
# This script binary-searches one or more IDs in one or more bins and prints
# id -> rva (or MISSING). Used to preflight REL::ID{og, ng, ae} triples offline
# before shipping them in the plugin (reference doc section 7 / section 41).
#
# Usage:
#   .\Check-ALIDs.ps1 -Ids 818081,2234796 -Bins 163,984,221
#   (bin shorthand: 163 980 984 137 221 -> version-1-10/11-*-0.bin)

param(
    [Parameter(Mandatory)] [uint64[]] $Ids,
    [string[]] $Bins = @("163", "980", "984", "137", "221"),
    [string] $BinDir = "e:\Fallout 4 Modding\F4SE\PluginTemplate\AddressLibrary\F4SE\Plugins"
)

function Resolve-BinPath([string] $short) {
    switch ($short) {
        "163" { return Join-Path $BinDir "version-1-10-163-0.bin" }
        "980" { return Join-Path $BinDir "version-1-10-980-0.bin" }
        "984" { return Join-Path $BinDir "version-1-10-984-0.bin" }
        "137" { return Join-Path $BinDir "version-1-11-137-0.bin" }
        "221" { return Join-Path $BinDir "version-1-11-221-0.bin" }
        default { return $short }  # allow a full path
    }
}

foreach ($bin in $Bins) {
    $path = Resolve-BinPath $bin
    if (-not (Test-Path $path)) { Write-Output "SKIP ${bin}: $path not found"; continue }

    $bytes = [System.IO.File]::ReadAllBytes($path)
    $count = [BitConverter]::ToUInt64($bytes, 0)

    foreach ($id in $Ids) {
        # Binary search over the ID-sorted entries
        [long]$lo = 0; [long]$hi = [long]$count - 1; $found = $false
        while ($lo -le $hi) {
            [long]$mid = ($lo + $hi) -shr 1
            $entryId = [BitConverter]::ToUInt64($bytes, 8 + 16 * $mid)
            if ($entryId -eq $id) {
                $rva = [BitConverter]::ToUInt64($bytes, 8 + 16 * $mid + 8)
                Write-Output ("{0,-4} id={1,-9} rva=0x{2:X}" -f $bin, $id, $rva)
                $found = $true; break
            } elseif ($entryId -lt $id) { $lo = $mid + 1 } else { $hi = $mid - 1 }
        }
        if (-not $found) { Write-Output ("{0,-4} id={1,-9} MISSING" -f $bin, $id) }
    }
}
