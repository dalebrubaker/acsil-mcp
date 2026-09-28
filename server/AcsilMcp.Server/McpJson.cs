using System.Text.Json;
using System.Text.Json.Serialization;

namespace AcsilMcp.Server;

public static class McpJson
{
    public static readonly JsonSerializerOptions Options = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
        WriteIndented = false
    };

    public static string Serialize<T>(T value) => JsonSerializer.Serialize(value, Options);

    /// <summary>Tool failures are returned as a normal result: <c>{"error":"..."}</c>.</summary>
    public static string Error(string message) => Serialize(new { error = message });
}
