#!/usr/bin/env python3
"""
  Copyright 2011 Jeff Garzik

  AuthServiceProxy has the following improvements over python-jsonrpc's
  ServiceProxy class:

  - HTTP connections persist for the life of the AuthServiceProxy object
    (if server supports HTTP/1.1)
  - sends protocol 'version', per JSON-RPC 1.1
  - sends proper, incrementing 'id'
  - sends Basic HTTP authentication headers
  - parses all JSON numbers that look like floats as Decimal
  - uses standard Python json lib

  Previous copyright, from python-jsonrpc/jsonrpc/proxy.py:

  Copyright (c) 2007 Jan-Klaas Kollhof

  This file is part of jsonrpc.

  jsonrpc is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this software; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
"""

import base64
import decimal
import json
import logging
from http.client import HTTPConnection, HTTPSConnection, BadStatusLine
from urllib.parse import urlparse

USER_AGENT = "AuthServiceProxy/0.1"

HTTP_TIMEOUT = 600

log = logging.getLogger("BitcoinRPC")

class JSONRPCException(Exception):
    def __init__(self, rpc_error):
        Exception.__init__(self, rpc_error.get("message"))
        self.error = rpc_error

def EncodeDecimal(o):
    if isinstance(o, decimal.Decimal):
        return str(o)
    raise TypeError(repr(o) + " is not JSON serializable")


class AuthServiceProxy():
    __id_count = 0

    def __init__(self, service_url, service_name=None, timeout=HTTP_TIMEOUT, connection=None):
        self.__service_url = service_url
        self._service_name = service_name
        self.__url =  urlparse(service_url)
        (user, passwd) = (self.__url.username, self.__url.password)
        try:
            user = user.encode('utf8')
        except AttributeError:
            pass
        try:
            passwd = passwd.encode('utf8')
        except AttributeError:
            pass
        authpair = user + b':' + passwd
        self.__auth_header = b'Basic ' + base64.b64encode(authpair)

        self.timeout = timeout
        self._set_conn(connection)

    def _set_conn(self, connection=None):
        port = 80 if self.__url.port is None else self.__url.port
        if connection:
            self.__conn = connection
            self.timeout = connection.timeout
        elif self.__url.scheme == 'https':
            self.__conn = HTTPSConnection(self.__url.hostname, port, timeout=self.timeout)
        else:
            self.__conn = HTTPConnection(self.__url.hostname, port, timeout=self.timeout)
   
    def __getattr__(self, name):
        if name.startswith('__') and name.endswith('__'):
            # Python internal stuff
            raise AttributeError
        if self._service_name is not None:
            name = "%s.%s" % (self._service_name, name)
        return AuthServiceProxy(self.__service_url, name, connection=self.__conn)

    def _request(self, method, path, postdata):
        '''
        Do a HTTP request, with retry if we get disconnected (e.g. due to a timeout).
        This is a workaround for https://bugs.python.org/issue3566 which is fixed in Python 3.5.
        '''
        headers = {'Host': self.__url.hostname,
                   'User-Agent': USER_AGENT,
                   'Authorization': self.__auth_header,
                   'Content-type': 'application/json'}
        try:
            self.__conn.request(method, path, postdata, headers)
            return self._get_response()
        except Exception as e:
            # If connection was closed, try again.
            # Python 3.5+ raises BrokenPipeError instead of BadStatusLine when the connection was reset.
            # ConnectionResetError happens on FreeBSD with Python 3.4.
            # This can be simplified now that we depend on Python 3 (previously, we could not
            # refer to BrokenPipeError or ConnectionResetError which did not exist on Python 2)
            if ((isinstance(e, BadStatusLine) and e.line == "''")
                or e.__class__.__name__ in ('BrokenPipeError', 'ConnectionResetError')):
                self.__conn.close()
                self.__conn.request(method, path, postdata, headers)
                return self._get_response()
            else:
                raise

    def __call__(self, *args):
        AuthServiceProxy.__id_count += 1

        log.debug("-%s-> %s %s"%(AuthServiceProxy.__id_count, self._service_name,
                                 json.dumps(args, default=EncodeDecimal)))
        postdata = json.dumps({'version': '1.1',
                               'method': self._service_name,
                               'params': args,
                               'id': AuthServiceProxy.__id_count}, default=EncodeDecimal)
        response = self._request('POST', self.__url.path, postdata)
        if response['error'] is not None:
            raise JSONRPCException(response['error'])
        elif 'result' not in response:
            raise JSONRPCException({
                'code': -343, 'message': 'missing JSON-RPC result'})
        else:
            return response['result']

    def _batch(self, rpc_call_list):
        postdata = json.dumps(list(rpc_call_list), default=EncodeDecimal)
        log.debug("--> "+postdata)
        return self._request('POST', self.__url.path, postdata)

    def _get_response(self):
        http_response = self.__conn.getresponse()
        if http_response is None:
            raise JSONRPCException({
                'code': -342, 'message': 'missing HTTP response from server'})
        
        content_type = http_response.getheader('Content-Type')
        if content_type != 'application/json':
            raise JSONRPCException({
                'code': -342, 'message': 'non-JSON HTTP response with \'%i %s\' from server' % (http_response.status, http_response.reason)})

        responsedata = http_response.read().decode('utf8')
        response = json.loads(responsedata, parse_float=decimal.Decimal)
        if "error" in response and response["error"] is None:
            log.debug("<-%s- %s"%(response["id"], json.dumps(response["result"], default=EncodeDecimal)))
        else:
            log.debug("<-- "+responsedata)
        return response
