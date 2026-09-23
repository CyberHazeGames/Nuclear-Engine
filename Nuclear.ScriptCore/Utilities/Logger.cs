using Nuclear.Managed.Interop;

namespace Nuclear.Utilities
{
    public class Logger
    {
        public static unsafe void Trace(object message)
        {
            using NativeString text = message?.ToString() ?? string.Empty;
            NativeCalls.LoggerTrace(text);
        }
        public static unsafe void Info(object message)
        {
            using NativeString text = message?.ToString() ?? string.Empty;
            NativeCalls.LoggerInfo(text);
        }
        public static unsafe void Warn(object message)
        {
            using NativeString text = message?.ToString() ?? string.Empty;
            NativeCalls.LoggerWarn(text);
        }
        public static unsafe void Error(object message)
        {
            using NativeString text = message?.ToString() ?? string.Empty;
            NativeCalls.LoggerError(text);
        }
        public static unsafe void Fatal(object message)
        {
            using NativeString text = message?.ToString() ?? string.Empty;
            NativeCalls.LoggerFatal(text);
        }
    }
}
