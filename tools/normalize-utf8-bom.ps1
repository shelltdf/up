param(
  [string]$Root = "."
)

$ErrorActionPreference = "Stop"

$extensions = @(
  ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".ipp", ".inl",
  ".md", ".xml", ".txt",
  ".cmake", ".ps1", ".bat", ".cmd", ".py", ".sh", ".json", ".yml", ".yaml"
)

$specialNames = @("CMakeLists.txt")
$utf8Bom = New-Object System.Text.UTF8Encoding($true)

Get-ChildItem -Path $Root -Recurse -File | ForEach-Object {
  $full = $_.FullName
  $name = $_.Name
  $ext = $_.Extension.ToLowerInvariant()
  if (($extensions -contains $ext) -or ($specialNames -contains $name)) {
    $content = [System.IO.File]::ReadAllText($full)
    [System.IO.File]::WriteAllText($full, $content, $utf8Bom)
  }
}

Write-Host "Normalized text/code files to UTF-8 BOM under $Root"
