# check-no-trading.ps1 — fails if acsil-mcp's own C++ sources reference ACSIL trading APIs.
#
# acsil-mcp must never place, modify, or cancel orders, change positions, or change the trade
# account (docs/safety.md). This guard scans native/core and native/adapter only; the vendored
# Sierra Chart headers in native/ACS_Source declare these APIs and are deliberately excluded.

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$dirs = @('native\core', 'native\adapter') | ForEach-Object { Join-Path $root $_ }
$pattern = '\b(BuyEntry|SellEntry|BuyExit|SellExit|BuyOrder|SellOrder|SubmitOrder|SubmitOCOOrder|ModifyOrder|CancelOrder|CancelAllOrders|FlattenPosition|FlattenAndCancelAllOrders|SelectedTradeAccount|SendOrdersToTradeService|AllowTradingOnHistoricalBars)\b'

$hits = Get-ChildItem -Path $dirs -Recurse -Include *.h, *.hpp, *.cpp |
    Select-String -Pattern $pattern

if ($hits) {
    $hits | ForEach-Object { Write-Host "$($_.Path):$($_.LineNumber): $($_.Line.Trim())" }
    Write-Host "Trading API referenced in acsil-mcp sources. See docs/safety.md."
    exit 1
}

Write-Host "No trading APIs referenced."
