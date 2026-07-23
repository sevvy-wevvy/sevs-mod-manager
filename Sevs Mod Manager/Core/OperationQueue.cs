namespace SevsModManager.Core;

internal enum OperationStatus { Queued, Running, Completed, Failed }

internal sealed class QueuedOperation
{
    public string Name = "";
    public OperationStatus Status = OperationStatus.Queued;
    public int Percent;
    public string StatusText = "";
    public string? Error;
    public DateTime QueuedAt = DateTime.Now;
    public DateTime? FinishedAt;
}

internal static class OperationQueue
{
    private const int MaxHistory = 25;
    private static readonly TimeSpan ProgressNotifyInterval = TimeSpan.FromMilliseconds(300);
    private static readonly SemaphoreSlim _gate = new(1, 1);
    private static readonly List<QueuedOperation> _history = new();
    private static DateTime _lastProgressNotify = DateTime.MinValue;

    public static event Action? Changed;
    public static IReadOnlyList<QueuedOperation> History => _history;
    public static QueuedOperation? Current { get; private set; }

    public static async Task RunAsync(string name, Func<IProgress<(int percent, string status)>, Task> work, IProgress<(int percent, string status)>? callerProgress = null)
    {
        var op = new QueuedOperation { Name = name };
        var progress = new Progress<(int percent, string status)>(p =>
        {
            op.Percent = p.percent;
            op.StatusText = p.status;
            callerProgress?.Report(p);

            var now = DateTime.UtcNow;
            if (now - _lastProgressNotify < ProgressNotifyInterval) return;
            _lastProgressNotify = now;
            Changed?.Invoke();
        });

        _history.Insert(0, op);
        if (_history.Count > MaxHistory) _history.RemoveAt(_history.Count - 1);
        Changed?.Invoke();

        if (_gate.CurrentCount == 0)
            callerProgress?.Report((0, "Waiting for another operation to finish..."));

        await _gate.WaitAsync();
        op.Status = OperationStatus.Running;
        Current = op;
        Changed?.Invoke();

        try
        {
            await Task.Run(() => work(progress));
            op.Status = OperationStatus.Completed;
        }
        catch (Exception ex)
        {
            op.Status = OperationStatus.Failed;
            op.Error = ex.Message;
            throw;
        }
        finally
        {
            op.FinishedAt = DateTime.Now;
            Current = null;
            _gate.Release();
            Changed?.Invoke();
        }
    }
}
