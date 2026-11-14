// Offline-patch a powered-off VDI: disable UAC so VBox Guest Control gets a full admin token.
// User-mode (DiscUtils) — no host Administrator, no RDP.
using DiscUtils;
using DiscUtils.Ntfs;
using DiscUtils.Registry;
using DiscUtils.Vdi;

if (args.Length < 1) {
    Console.Error.WriteLine("usage: VdiGuestElevate <path-to.vdi>");
    return 2;
}

var vdi = args[0];
if (!File.Exists(vdi)) {
    Console.Error.WriteLine("missing " + vdi);
    return 2;
}

using var disk = new Disk(vdi, FileAccess.ReadWrite);
Console.WriteLine($"[*] VDI capacity={disk.Capacity} sectorsize={disk.SectorSize}");
try {
    var pt = disk.Partitions;
    Console.WriteLine($"[*] partition count={pt?.Count}");
    if (pt != null) {
        for (var i = 0; i < pt.Count; i++) {
            var p = pt.Partitions[i];
            Console.WriteLine($"    [{i}] type={p.TypeAsString} first={p.FirstSector} count={p.SectorCount} bios=0x{p.BiosType:X2}");
        }
    }
} catch (Exception ex) {
    Console.WriteLine("[!] partition table: " + ex.Message);
}

var mgr = new VolumeManager(disk);
var vols = mgr.GetLogicalVolumes();
Console.WriteLine($"[*] logical volumes={vols.Length}");

NtfsFileSystem? ntfs = null;
Stream? keepOpen = null;
string? hiveRel = null;
string[] hivePaths = [
    @"Windows\System32\config\SOFTWARE",
    @"\Windows\System32\config\SOFTWARE",
];

foreach (var vol in vols) {
    Console.WriteLine($"    vol identity={vol.Identity} length={vol.Length} bios=0x{vol.BiosType:X2}");
    Stream opened;
    try { opened = vol.Open(); }
    catch (Exception ex) {
        Console.WriteLine("      open failed: " + ex.Message);
        continue;
    }
    try {
        var sig = new byte[16];
        opened.Position = 0;
        var n = opened.Read(sig, 0, 16);
        opened.Position = 0;
        var oem = System.Text.Encoding.ASCII.GetString(sig, 3, Math.Max(0, n - 3));
        Console.WriteLine("      boot=" + BitConverter.ToString(sig, 0, n) + " oem=[" + oem + "]");
        var fs = new NtfsFileSystem(opened);
        try {
            var names = string.Join(",", fs.GetDirectories("\\").Take(12));
            Console.WriteLine("      ntfs root: " + names);
        } catch { /* ignore listing */ }
        foreach (var hp in hivePaths) {
            if (!fs.FileExists(hp)) continue;
            Console.WriteLine("      hive at " + hp);
            ntfs = fs;
            keepOpen = opened;
            hiveRel = hp.TrimStart('\\');
            break;
        }
        if (ntfs != null) break;
        fs.Dispose();
        opened.Dispose();
    } catch (Exception ex) {
        Console.WriteLine("      not ntfs: " + ex.GetType().Name + " " + ex.Message);
        opened.Dispose();
    }
}

if (ntfs is null || hiveRel is null) {
    Console.Error.WriteLine("no Windows SOFTWARE hive in VDI");
    return 3;
}

try {
    PatchHive(ntfs, hiveRel, PatchSoftware);
    Console.WriteLine("[+] SOFTWARE hive patched (UAC off + token filter policy)");
} finally {
    ntfs.Dispose();
    keepOpen?.Dispose();
}

return 0;

static void PatchSoftware(RegistryHive hive)
{
    var key = hive.Root.OpenSubKey(@"Microsoft\Windows\CurrentVersion\Policies\System")
              ?? hive.Root.CreateSubKey(@"Microsoft\Windows\CurrentVersion\Policies\System");
    key.SetValue("EnableLUA", 0, RegistryValueType.Dword);
    key.SetValue("ConsentPromptBehaviorAdmin", 0, RegistryValueType.Dword);
    key.SetValue("ConsentPromptBehaviorUser", 0, RegistryValueType.Dword);
    key.SetValue("PromptOnSecureDesktop", 0, RegistryValueType.Dword);
    key.SetValue("FilterAdministratorToken", 0, RegistryValueType.Dword);
    key.SetValue("EnableInstallerDetection", 0, RegistryValueType.Dword);
    key.SetValue("LocalAccountTokenFilterPolicy", 1, RegistryValueType.Dword);
}

static void PatchHive(NtfsFileSystem ntfs, string hivePath, Action<RegistryHive> patch)
{
    using var stream = ntfs.OpenFile(hivePath, FileMode.Open, FileAccess.ReadWrite);
    using var hive = new RegistryHive(stream);
    patch(hive);
}
