/**
 *  \file
 *  \brief  JSON object log message formatter
 *
 *  \date   2017/07/15
 *  \author Vaclav Krpec  <vencik@razdva.cz>
 *
 *
 *  LEGAL NOTICE
 *
 *  Copyright (c) 2017, Vaclav Krpec
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the distribution.
 *
 *  3. Neither the name of the copyright holder nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
 *  OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <libprocxx/logging/json_formatter.hxx>

#include <sstream>
#include <iomanip>


namespace libprocxx {
namespace logging {

/** Escape special characters to suit JSON format */
static void json_escape(
    std::istream & istream,
    std::ostream & ostream)
{
    for (char c; istream.get(c); ) {
        switch (c) {
            // Special characters
            case '"':  ostream << "\\\""; break;
            case '\\': ostream << "\\\\"; break;
            case '\b': ostream << "\\b";  break;
            case '\f': ostream << "\\f";  break;
            case '\n': ostream << "\\n";  break;
            case '\r': ostream << "\\r";  break;
            case '\t': ostream << "\\t";  break;

            default:
                // Other special characters
                if ('\x00' <= c && c <= '\x1f')
                    ostream
                        << "\\u"
                        << std::hex << std::setw(4) << std::setfill('0')
                        << (int)c;

                // Plain character
                else
                    ostream << c;
        }
    }
}


/** Escape special characters in string */
static void json_escape(
    const std::string & str,
    std::ostream &      ostream)
{
    std::stringstream ss(str);
    json_escape(ss, ostream);
}


void json_formatter::format(
    worker::message & msg,
    std::ostream & ostream)
{
    auto usecs_since_epoch =
        std::chrono::duration_cast<std::chrono::microseconds>(
            msg.time.time_since_epoch()).count();
    double secs_since_epoch = (double)usecs_since_epoch / 1000000.0;

    ostream << "{\"logger_name\":\"";
    json_escape(msg.logger->name, ostream);

    ostream
        << "\",\"level\":" << msg.level
        << ",\"level_str\":\""
        << libprocxx::logging::log_level2str(msg.level)
        << "\",\"timestamp\":" << std::fixed << secs_since_epoch
        << ",\"file\":\"";
    json_escape(msg.file, ostream);

    ostream
        << "\",\"line\":" << msg.line
        << ",\"function\":\"";
    json_escape(msg.function, ostream);

    ostream << "\",\"message\":\"";
    json_escape(msg.msg_ss, ostream);

    ostream << "\"}" << std::endl;
}

}}  // end of namespaces logging libprocxx
