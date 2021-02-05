#pragma once
#ifndef _uefi_protos_http_h_
#define _uefi_protos_http_h_

// This file was auto generated by the j6-uefi-headers project. Please see
// https://github.com/justinian/j6-uefi-headers for more information.

#include <uefi/guid.h>
#include <uefi/types.h>
#include <uefi/networking.h>

namespace uefi {
namespace protos {
struct http;

struct http
{
    static constexpr uefi::guid guid{ 0x7a59b29b,0x910b,0x4171,{0x82,0x42,0xa8,0x5a,0x0d,0xf2,0x5b,0x5b} };
    static constexpr uefi::guid service_binding{ 0xbdc8e6af,0xd9bc,0x4379,{0xa7,0x2a,0xe0,0xc4,0xe7,0x5d,0xae,0x1c} };


    inline uefi::status get_mode_data(uefi::http_config_data * http_config_data) {
        return _get_mode_data(this, http_config_data);
    }

    inline uefi::status configure(uefi::http_config_data * http_config_data) {
        return _configure(this, http_config_data);
    }

    inline uefi::status request(uefi::http_token * token) {
        return _request(this, token);
    }

    inline uefi::status cancel(uefi::http_token * token) {
        return _cancel(this, token);
    }

    inline uefi::status response(uefi::http_token * token) {
        return _response(this, token);
    }

    inline uefi::status poll() {
        return _poll(this);
    }


protected:
    using _get_mode_data_def = uefi::status (*)(uefi::protos::http *, uefi::http_config_data *);
    _get_mode_data_def _get_mode_data;

    using _configure_def = uefi::status (*)(uefi::protos::http *, uefi::http_config_data *);
    _configure_def _configure;

    using _request_def = uefi::status (*)(uefi::protos::http *, uefi::http_token *);
    _request_def _request;

    using _cancel_def = uefi::status (*)(uefi::protos::http *, uefi::http_token *);
    _cancel_def _cancel;

    using _response_def = uefi::status (*)(uefi::protos::http *, uefi::http_token *);
    _response_def _response;

    using _poll_def = uefi::status (*)(uefi::protos::http *);
    _poll_def _poll;

public:
};

} // namespace protos
} // namespace uefi

#endif // _uefi_protos_http_h_