# fetch-acsil-docs.ps1 — downloads Sierra Chart's ACSIL documentation and strips it to plain text.
#
# The pages are Sierra Chart's copyrighted text, so the output goes to a git-ignored folder
# (docs\acsil-cache by default) and is never committed. Search it instead of asking a summarizer:
#   Select-String -Path docs\acsil-cache\*.txt -Pattern 'GetBarHasClosedStatus' -Context 2,8
#
# The page list is read from the live Contents page (the ACSIL section), so it follows the site.

param(
    [string]$OutDir = (Join-Path (Split-Path -Parent $PSScriptRoot) 'docs\acsil-cache'),
    [int]$DelayMs = 1000,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
$site = 'https://www.sierrachart.com'
$sectionStart = 'id="AdvancedCustomStudySystemInterfaceAndLanguage"'
$sectionEnd = 'id="UsingSpreadsheets"'

function Get-Page([string]$Url) {
    $tmp = New-TemporaryFile
    try {
        $code = & curl.exe -s -L --fail -o $tmp.FullName -w '%{http_code}' $Url
        if ($LASTEXITCODE -ne 0) { throw "GET $Url failed (curl exit $LASTEXITCODE, HTTP $code)" }
        return [System.IO.File]::ReadAllText($tmp.FullName, [System.Text.Encoding]::UTF8)
    } finally {
        Remove-Item $tmp.FullName -ErrorAction SilentlyContinue
    }
}

function ConvertTo-PlainText([string]$Html) {
    $t = $Html
    $t = [regex]::Replace($t, '(?is)<(script|style|noscript)\b.*?</\1>', '')
    $t = [regex]::Replace($t, '(?is)<!--.*?-->', '')
    $t = [regex]::Replace($t, '(?is)<h([1-6])\b[^>]*>(.*?)</h\1>', { param($m) "`n`n" + ('#' * [int]$m.Groups[1].Value) + ' ' + $m.Groups[2].Value + "`n" })
    $t = [regex]::Replace($t, '(?is)<li\b[^>]*>', "`n- ")
    $t = [regex]::Replace($t, '(?is)<(br|/p|/div|/tr|/table|/pre|/ul|/ol|/section)\b[^>]*>', "`n")
    $t = [regex]::Replace($t, '(?is)<(p|div|pre|tr|table|section)\b[^>]*>', "`n")
    $t = [regex]::Replace($t, '(?is)</t[dh]>', "`t")
    $t = [regex]::Replace($t, '(?s)<[^>]+>', '')
    $t = [System.Net.WebUtility]::HtmlDecode($t)
    $t = $t -replace "`r", ''
    $t = [regex]::Replace($t, '[  ]+\n', "`n")
    $t = [regex]::Replace($t, '\n{3,}', "`n`n")
    return $t.Trim() + "`n"
}

New-Item -ItemType Directory -Force $OutDir | Out-Null

$contents = Get-Page "$site/index.php?page=doc/Contents.php"
$start = $contents.IndexOf($sectionStart)
$end = $contents.IndexOf($sectionEnd)
if ($start -lt 0 -or $end -le $start) {
    throw 'ACSIL section not found on the Contents page; the site layout changed. Update $sectionStart/$sectionEnd.'
}

$pages = [regex]::Matches($contents.Substring($start, $end - $start), 'href="(/index\.php\?page=doc/([^"#&]+))"') |
    ForEach-Object { [pscustomobject]@{ Path = $_.Groups[1].Value; Name = [IO.Path]::GetFileNameWithoutExtension($_.Groups[2].Value) } } |
    Sort-Object Path -Unique

Write-Host "$($pages.Count) ACSIL pages -> $OutDir"
$fetched = Get-Date -Format 'yyyy-MM-dd'

foreach ($p in $pages) {
    $out = Join-Path $OutDir "$($p.Name).txt"
    if ((Test-Path $out) -and -not $Force) {
        Write-Host "skip  $($p.Name) (exists; use -Force to refresh)"
        continue
    }
    $html = Get-Page ($site + $p.Path)
    $text = ConvertTo-PlainText $html
    $header = "Source: $site$($p.Path)`nFetched: $fetched`n`n"
    [System.IO.File]::WriteAllText($out, $header + $text, [System.Text.UTF8Encoding]::new($false))
    Write-Host ("wrote {0} ({1:N0} KB)" -f $p.Name, ((Get-Item $out).Length / 1KB))
    Start-Sleep -Milliseconds $DelayMs
}
