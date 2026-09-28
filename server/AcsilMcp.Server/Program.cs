using AcsilMcp.Server;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using ModelContextProtocol.Protocol;

var builder = Host.CreateApplicationBuilder(args);

// stdout carries the MCP protocol; logs must go to stderr.
builder.Logging.ClearProviders();
builder.Logging.AddConsole(options => options.LogToStandardErrorThreshold = LogLevel.Trace);

builder.Services.AddSingleton(AgentClientOptions.FromCommandLine(args));
builder.Services.AddSingleton<AgentClient>();

builder.Services
    .AddMcpServer(options =>
    {
        options.ServerInfo = new Implementation
        {
            Name = ServerInfo.Name,
            Version = ServerInfo.Version
        };
    })
    .WithStdioServerTransport()
    .WithToolsFromAssembly(typeof(ServerInfo).Assembly);

await builder.Build().RunAsync().ConfigureAwait(false);
