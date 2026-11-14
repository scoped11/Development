// Runs elevated (via SilentCleanup / fodhelper hijack) to register VM lab workers. No UI.
var scripts = new[] {
    @"Z:\byovd\driver3\register-driver3-elevated-task.ps1",
    @"D:\hostdev\byovd\driver3\register-driver3-elevated-task.ps1",
};
var marker = @"C:\DriverLab\guest-elevated.json";
Directory.CreateDirectory(@"C:\DriverLab");
var log = new List<string>();
foreach (var s in scripts) {
    if (!File.Exists(s)) { log.Add("missing " + s); continue; }
    var p = System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo {
        FileName = @"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe",
        Arguments = "-NoProfile -ExecutionPolicy Bypass -File \"" + s + "\"",
        UseShellExecute = false,
        CreateNoWindow = true,
    });
    p?.WaitForExit(120_000);
    log.Add($"{s} exit={p?.ExitCode}");
}
try {
    var p2 = System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo {
        FileName = @"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe",
        Arguments = "-NoProfile -ExecutionPolicy Bypass -Command \"$env:DRIVER2_VM_LAB_GUEST='1'; $env:DRIVER2_VM_REGISTER_TASK='1'; & 'Z:\\byovd\\driver2\\vm-lab.ps1'\"",
        UseShellExecute = false,
        CreateNoWindow = true,
    });
    p2?.WaitForExit(120_000);
    log.Add("vm-lab register exit=" + p2?.ExitCode);
} catch (Exception ex) { log.Add("vm-lab: " + ex.Message); }

File.WriteAllText(marker, System.Text.Json.JsonSerializer.Serialize(new {
    utc = DateTime.UtcNow.ToString("o"),
    log,
    admin = new System.Security.Principal.WindowsPrincipal(
        System.Security.Principal.WindowsIdentity.GetCurrent())
        .IsInRole(System.Security.Principal.WindowsBuiltInRole.Administrator),
}));
