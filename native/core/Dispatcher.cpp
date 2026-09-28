#include "core/Dispatcher.h"

#include "core/Version.h"

namespace acsilmcp
{
    namespace
    {
        Json Handle(const DispatchContext& context, const Json& request)
        {
            if (!request.is_object() || !request.contains("cmd") || !request["cmd"].is_string())
            {
                return ErrorReply("request must be an object with a string \"cmd\"");
            }

            const auto cmd = request["cmd"].get<std::string>();
            if (cmd == "PING")
            {
                return OkReply(Json{
                    {"protocolVersion", kProtocolVersion},
                    {"instanceId", context.instanceId},
                    {"agentCount", context.registry.Count()},
                });
            }
            if (cmd == "LIST_CHARTS")
            {
                return OkReply(Json{{"charts", context.registry.ListCharts()}});
            }
            if (request.contains("chart"))
            {
                if (!request["chart"].is_string())
                {
                    return ErrorReply("\"chart\" must be a chart id string from LIST_CHARTS");
                }
                return context.registry.Call(request["chart"].get<std::string>(), request, context.chartTimeout);
            }
            return ErrorReply("unknown command " + cmd);
        }
    }

    std::string Dispatch(const DispatchContext& context, const std::string& requestPayload)
    {
        Json request;
        Json reply;
        try
        {
            request = Json::parse(requestPayload);
            reply = Handle(context, request);
        }
        catch (const std::exception& ex)
        {
            reply = ErrorReply(std::string("bad request: ") + ex.what());
        }

        if (request.is_object() && request.contains("id"))
        {
            reply["id"] = request["id"];
        }
        return reply.dump(-1, ' ', false, Json::error_handler_t::replace);
    }
}
