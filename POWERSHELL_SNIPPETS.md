# PowerShell Snippets

## Fast file search across Windows drives

Replace the value in `$p` when you want to search for a different filename fragment.

```powershell
$p='*B2D93A*'
$ex='^(?:[A-Z]:\\(?:Windows|Program Files(?: \(x86\))?|ProgramData|OneDriveTemp|\$WINDOWS\.~BT))(\\|$)'; Get-PSDrive -PSProvider FileSystem | % { $r=$_.Root; Get-ChildItem -Path $r -File -Filter $p -ErrorAction SilentlyContinue; Get-ChildItem -Path $r -Directory -Force -ErrorAction SilentlyContinue | ? { $_.FullName -notmatch $ex } | % { Get-ChildItem -Path $_.FullName -Recurse -File -Filter $p -ErrorAction SilentlyContinue } } | Select-Object -ExpandProperty FullName
```
