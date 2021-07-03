#ifndef HandleMessage_h
#define HandleMessage_h

#include "Arguments.h"
#include "MessageDecoder.h"
#include "MessageEncoder.h"
#include <wtf/StdLibExtras.h>

namespace IPC {

// Dispatch functions with no reply arguments.

template <typename C, typename MF, typename ArgsTuple, size_t... ArgsIndex>
void callMemberFunctionImpl(C* object, MF function, ArgsTuple&& args, std::index_sequence<ArgsIndex...>)
{
    (object->*function)(std::get<ArgsIndex>(std::forward<ArgsTuple>(args))...);
}

template<typename C, typename MF, typename ArgsTuple, typename ArgsIndicies = std::make_index_sequence<std::tuple_size<ArgsTuple>::value>>
void callMemberFunction(ArgsTuple&& args, C* object, MF function)
{
    callMemberFunctionImpl(object, function, std::forward<ArgsTuple>(args), ArgsIndicies());
}

// Dispatch functions with reply arguments.

template <typename C, typename MF, typename ArgsTuple, size_t... ArgsIndex, typename ReplyArgsTuple, size_t... ReplyArgsIndex>
void callMemberFunctionImpl(C* object, MF function, ArgsTuple&& args, ReplyArgsTuple& replyArgs, std::index_sequence<ArgsIndex...>, std::index_sequence<ReplyArgsIndex...>)
{
    (object->*function)(std::get<ArgsIndex>(std::forward<ArgsTuple>(args))..., std::get<ReplyArgsIndex>(replyArgs)...);
}

template <typename C, typename MF, typename ArgsTuple, typename ArgsIndicies = std::make_index_sequence<std::tuple_size<ArgsTuple>::value>, typename ReplyArgsTuple, typename ReplyArgsIndicies = std::make_index_sequence<std::tuple_size<ReplyArgsTuple>::value>>
void callMemberFunction(ArgsTuple&& args, ReplyArgsTuple& replyArgs, C* object, MF function)
{
    callMemberFunctionImpl(object, function, std::forward<ArgsTuple>(args), replyArgs, ArgsIndicies(), ReplyArgsIndicies());
}

// Dispatch functions with delayed reply arguments.

template <typename C, typename MF, typename R, typename ArgsTuple, size_t... ArgsIndex>
void callMemberFunctionImpl(C* object, MF function, PassRefPtr<R> delayedReply, ArgsTuple&& args, std::index_sequence<ArgsIndex...>)
{
    (object->*function)(std::get<ArgsIndex>(args)..., delayedReply);
}

template<typename C, typename MF, typename R, typename ArgsTuple, typename ArgsIndicies = std::make_index_sequence<std::tuple_size<ArgsTuple>::value>>
void callMemberFunction(ArgsTuple&& args, PassRefPtr<R> delayedReply, C* object, MF function)
{
    callMemberFunctionImpl(object, function, delayedReply, std::forward<ArgsTuple>(args), ArgsIndicies());
}

// Dispatch functions with connection parameter with no reply arguments.

template <typename C, typename MF, typename ArgsTuple, size_t... ArgsIndex>
void callMemberFunctionImpl(C* object, MF function, Connection& connection, ArgsTuple&& args, std::index_sequence<ArgsIndex...>)
{
    (object->*function)(connection, std::get<ArgsIndex>(args)...);
}

template<typename C, typename MF, typename ArgsTuple, typename ArgsIndicies = std::make_index_sequence<std::tuple_size<ArgsTuple>::value>>
void callMemberFunction(Connection& connection, ArgsTuple&& args, C* object, MF function)
{
    callMemberFunctionImpl(object, function, connection, std::forward<ArgsTuple>(args), ArgsIndicies());
}

// Dispatch functions with connection parameter with reply arguments.

template <typename C, typename MF, typename ArgsTuple, size_t... ArgsIndex, typename ReplyArgsTuple, size_t... ReplyArgsIndex>
void callMemberFunctionImpl(C* object, MF function, Connection& connection, ArgsTuple&& args, ReplyArgsTuple& replyArgs, std::index_sequence<ArgsIndex...>, std::index_sequence<ReplyArgsIndex...>)
{
    (object->*function)(connection, std::get<ArgsIndex>(std::forward<ArgsTuple>(args))..., std::get<ReplyArgsIndex>(replyArgs)...);
}

template <typename C, typename MF, typename ArgsTuple, typename ArgsIndicies = std::make_index_sequence<std::tuple_size<ArgsTuple>::value>, typename ReplyArgsTuple, typename ReplyArgsIndicies = std::make_index_sequence<std::tuple_size<ReplyArgsTuple>::value>>
void callMemberFunction(Connection& connection, ArgsTuple&& args, ReplyArgsTuple& replyArgs, C* object, MF function)
{
    callMemberFunctionImpl(object, function, connection, std::forward<ArgsTuple>(args), replyArgs, ArgsIndicies(), ReplyArgsIndicies());
}

// Main dispatch functions

template<typename T, typename C, typename MF>
void handleMessage(MessageDecoder& decoder, C* object, MF function)
{
    typename T::DecodeType arguments;
    if (!decoder.decode(arguments)) {
        ASSERT(decoder.isInvalid());
        return;
    }

    callMemberFunction(WTF::move(arguments), object, function);
}

template<typename T, typename C, typename MF>
void handleMessage(MessageDecoder& decoder, MessageEncoder& replyEncoder, C* object, MF function)
{
    typename T::DecodeType arguments;
    if (!decoder.decode(arguments)) {
        ASSERT(decoder.isInvalid());
        return;
    }

    typename T::Reply::ValueType replyArguments;
    callMemberFunction(WTF::move(arguments), replyArguments, object, function);
    replyEncoder << replyArguments;
}

template<typename T, typename C, typename MF>
void handleMessage(Connection& connection, MessageDecoder& decoder, MessageEncoder& replyEncoder, C* object, MF function)
{
    typename T::DecodeType arguments;
    if (!decoder.decode(arguments)) {
        ASSERT(decoder.isInvalid());
        return;
    }

    typename T::Reply::ValueType replyArguments;
    callMemberFunction(connection, WTF::move(arguments), replyArguments, object, function);
    replyEncoder << replyArguments;
}

template<typename T, typename C, typename MF>
void handleMessage(Connection& connection, MessageDecoder& decoder, C* object, MF function)
{
    typename T::DecodeType arguments;
    if (!decoder.decode(arguments)) {
        ASSERT(decoder.isInvalid());
        return;
    }
    callMemberFunction(connection, WTF::move(arguments), object, function);
}

template<typename T, typename C, typename MF>
void handleMessageDelayed(Connection& connection, MessageDecoder& decoder, std::unique_ptr<MessageEncoder>& replyEncoder, C* object, MF function)
{
    typename T::DecodeType arguments;
    if (!decoder.decode(arguments)) {
        ASSERT(decoder.isInvalid());
        return;
    }

    RefPtr<typename T::DelayedReply> delayedReply = adoptRef(new typename T::DelayedReply(&connection, WTF::move(replyEncoder)));
    callMemberFunction(WTF::move(arguments), delayedReply.release(), object, function);
}

} // namespace IPC

#endif // HandleMessage_h
