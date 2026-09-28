namespace AcsilMcp.Server;

/// <summary>
/// Formats Sierra Chart's bar period (s_BarPeriod: ChartDataTypeEnum, IntradayBarPeriodTypeEnum,
/// IntradayChartBarPeriodParameter1) the way the chart title shows it, for example "1 Min".
/// </summary>
public static class BarPeriod
{
    private const int DailyData = 1;

    public static string Format(int chartDataType, int barPeriodType, int param1)
    {
        if (chartDataType == DailyData)
        {
            return "Daily";
        }

        return barPeriodType switch
        {
            0 => FormatTime(param1),
            1 => $"{param1} Volume",
            2 => $"{param1} Trades",
            3 or 4 or 5 or 6 or 11 or 17 => $"{param1} Range",
            7 => $"{param1} Reversal",
            8 or 10 or 15 or 16 => $"{param1} Renko",
            9 => $"{param1} Delta Volume",
            12 => $"{param1} Price Changes",
            13 => $"{param1} Months",
            14 => "Point & Figure",
            18 => "Custom",
            10000 => $"{param1} Days",
            _ => $"type {barPeriodType}, {param1}"
        };
    }

    private static string FormatTime(int seconds)
    {
        if (seconds > 0 && seconds % 86400 == 0)
        {
            return $"{seconds / 86400} Day";
        }
        if (seconds > 0 && seconds % 60 == 0)
        {
            return $"{seconds / 60} Min";
        }
        return $"{seconds} Sec";
    }
}
