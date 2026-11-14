using System.Diagnostics;
using System.IO;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;

namespace DriverLabLoader;

public partial class MainWindow : Window
{
    readonly string _labRoot;
    readonly string _byovdRoot;
    readonly DispatcherTimer _gifTimer = new();
    int _gifFrame;
    GifAnimator? _gif;

    public MainWindow()
    {
        InitializeComponent();
        _labRoot = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, ".."));
        _byovdRoot = Path.GetFullPath(Path.Combine(_labRoot, "..", "byovd"));
        Loaded += OnLoaded;
    }

    void OnLoaded(object sender, RoutedEventArgs e)
    {
        var assets = Path.Combine(_labRoot, "assets");
        var mp4 = Path.Combine(assets, "loading_animation.mp4");
        var gif = Path.Combine(assets, "loading_animation.gif");
        if (File.Exists(mp4))
        {
            SplashVideo.Source = new Uri(mp4);
            SplashVideo.Visibility = Visibility.Visible;
            SplashGif.Visibility = Visibility.Collapsed;
            SplashVideo.Play();
        }
        else if (File.Exists(gif))
        {
            _gif = new GifAnimator(gif);
            SplashVideo.Visibility = Visibility.Collapsed;
            SplashGif.Visibility = Visibility.Visible;
            _gifTimer.Interval = TimeSpan.FromMilliseconds(42);
            _gifTimer.Tick += (_, _) => TickGif();
            _gifTimer.Start();
            TickGif();
        }
        else
        {
            StageText.Text = "Place loading_animation.gif or .mp4 in driver-lab/assets/";
        }
    }

    void SplashVideo_MediaEnded(object sender, RoutedEventArgs e)
    {
        SplashVideo.Position = TimeSpan.Zero;
        SplashVideo.Play();
    }

    void TickGif()
    {
        if (_gif is null) return;
        var frame = _gif.GetFrame(_gifFrame++ % _gif.FrameCount);
        SplashGif.Source = frame;
    }

    void SetStage(string text, double pct)
    {
        StageText.Text = text;
        ProgressBar.Value = pct;
    }

    async Task RunScriptAsync(string relativeFromByovd, string label)
    {
        var script = Path.Combine(_byovdRoot, relativeFromByovd.Replace('/', Path.DirectorySeparatorChar));
        if (!File.Exists(script))
        {
            MessageBox.Show($"Missing:\n{script}\n\nBuild byovd first.", "Driver Lab", MessageBoxButton.OK, MessageBoxImage.Warning);
            return;
        }
        SetStage(label, 15);
        ProcessStartInfo psi;
        if (script.EndsWith(".cmd", StringComparison.OrdinalIgnoreCase))
        {
            psi = new ProcessStartInfo
            {
                FileName = "cmd.exe",
                Arguments = $"/c \"{script}\"",
                WorkingDirectory = _byovdRoot,
                UseShellExecute = false,
                CreateNoWindow = true,
            };
        }
        else
        {
            psi = new ProcessStartInfo
            {
                FileName = "powershell.exe",
                Arguments = $"-NoProfile -ExecutionPolicy Bypass -File \"{script}\"",
                WorkingDirectory = _byovdRoot,
                UseShellExecute = false,
                CreateNoWindow = true,
            };
        }
        using var p = Process.Start(psi);
        if (p is null) return;
        while (!p.HasExited)
        {
            await Task.Delay(200);
            SetStage($"{label}…", Math.Min(90, ProgressBar.Value + 2));
        }
        SetStage(p.ExitCode == 0 ? $"{label} — OK" : $"{label} — exit {p.ExitCode}", p.ExitCode == 0 ? 100 : 20);
        if (p.ExitCode != 0)
            MessageBox.Show($"Exit code {p.ExitCode}. See %TEMP%\\pf_diag.tmp", "Driver Lab", MessageBoxButton.OK, MessageBoxImage.Information);
    }

    async void OnSecureMap(object sender, RoutedEventArgs e) =>
        await RunScriptAsync("map-secure-rwtest.ps1", "Secure RwTest map");

    async void OnRustBypass(object sender, RoutedEventArgs e) =>
        await RunScriptAsync("run-bypass-rust-replica.cmd", "Rust replica bypass");

    async void OnR6Bypass(object sender, RoutedEventArgs e) =>
        await RunScriptAsync("run-bypass-r6-replica.cmd", "R6 replica bypass");

    async void OnRwTest(object sender, RoutedEventArgs e)
    {
        var exe = Path.Combine(_byovdRoot, "bin", "process_rw.exe");
        if (!File.Exists(exe))
        {
            MessageBox.Show("Build byovd: .\\build.ps1", "Driver Lab", MessageBoxButton.OK, MessageBoxImage.Warning);
            return;
        }
        SetStage("process_rw smoke test", 50);
        var psi = new ProcessStartInfo { FileName = exe, WorkingDirectory = Path.Combine(_byovdRoot, "bin"), UseShellExecute = false };
        using var p = Process.Start(psi);
        p?.WaitForExit();
        SetStage(p?.ExitCode == 0 ? "R/W OK" : "R/W failed", p?.ExitCode == 0 ? 100 : 25);
    }

    void OnClose(object sender, RoutedEventArgs e) => Close();
}

/// <summary>Decode GIF frames via WPF BitmapDecoder (first pass — single frame if animated metadata limited).</summary>
sealed class GifAnimator
{
    readonly BitmapDecoder _dec;
    public int FrameCount => _dec.Frames.Count;

    public GifAnimator(string path)
    {
        using var s = File.OpenRead(path);
        _dec = BitmapDecoder.Create(s, BitmapCreateOptions.None, BitmapCacheOption.OnLoad);
    }

    public BitmapSource GetFrame(int index)
    {
        var f = _dec.Frames[Math.Clamp(index, 0, FrameCount - 1)];
        return BitmapFrame.Create(f);
    }
}
