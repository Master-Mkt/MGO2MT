namespace MgoUi;

internal static class Program
{
    [STAThread] static void Main(string[] args)
    {
        ApplicationConfiguration.Initialize(); Application.SetUnhandledExceptionMode(UnhandledExceptionMode.CatchException);
        Application.ThreadException += (_, e) =>
        {
            foreach(var flag in new[]{"--verify-audio","--capture","--la2-output"})
            {
                var i=Array.IndexOf(args,flag);if(i<0||i+1>=args.Length)continue;
                var folder=Path.GetFullPath(args[i+1]);Directory.CreateDirectory(folder);File.WriteAllText(Path.Combine(folder,"unhandled-error.txt"),e.Exception.ToString());Environment.Exit(1);return;
            }
            MessageBox.Show(e.Exception.Message,"操作を完了できません",MessageBoxButtons.OK,MessageBoxIcon.Warning);
        };
        Application.Run(new MainForm(args));
    }
}
