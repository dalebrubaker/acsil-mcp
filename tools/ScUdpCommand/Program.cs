// ScUdpCommand — sends one UDP command to a local Sierra Chart instance.
//
// Used by native\AcsilMcp.vcxproj: RELEASE_ALL_DLLS before a build so the DLL file is unlocked,
// ALLOW_LOAD_ALL_DLLS after copying the new DLL. Enable the listener in Sierra Chart under
// Global Settings >> Sierra Chart Server Settings >> UDP Port.

using System.Net;
using System.Net.Sockets;
using System.Text;

if (args.Length < 1)
{
    Console.Error.WriteLine("Usage: ScUdpCommand <command> [port] [delayMs]");
    Console.Error.WriteLine("  command  : RELEASE_ALL_DLLS or ALLOW_LOAD_ALL_DLLS");
    Console.Error.WriteLine("  port     : UDP port (default 22902)");
    Console.Error.WriteLine("  delayMs  : milliseconds to wait after sending (default 500)");
    return 1;
}

var command = args[0];
var port = args.Length >= 2 ? int.Parse(args[1]) : 22902;
var delayMs = args.Length >= 3 ? int.Parse(args[2]) : 500;

try
{
    var data = Encoding.ASCII.GetBytes(command);
    using var client = new UdpClient();
    // 127.0.0.1, not "localhost": localhost may resolve to ::1 and Sierra Chart listens on IPv4 only.
    client.Send(data, data.Length, new IPEndPoint(IPAddress.Loopback, port));
    Console.WriteLine($"Sent \"{command}\" to 127.0.0.1:{port}");

    if (delayMs > 0)
    {
        Thread.Sleep(delayMs);
    }

    return 0;
}
catch (Exception ex)
{
    // Never fail the build because Sierra Chart is not running.
    Console.Error.WriteLine($"Warning: UDP send failed: {ex}");
    return 0;
}
