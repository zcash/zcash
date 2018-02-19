import sys
import json
import subprocess


class RPCError(IOError):
    pass


class JsonClient(object):
    def __getattr__(self, method):
        def inner(*args):
            return self._exec(method, args)
        return inner

    def load_response(self, data):
        data = json.loads(data)
        if data.get('error'):
            raise RPCError(data['error'])
        if 'result' in data:
            return data['result']
        return data


def run_cmd(cmd):
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    #print >>sys.stderr, "> %s" % repr(cmd)[1:-1]
    assert proc.wait() == 0
    return proc.stdout.read()


class Hoek(JsonClient):
    def _exec(self, method, args):
        return self.load_response(run_cmd(['hoek', method, json.dumps(args[0])]))


class Komodod(JsonClient):
    def __init__(self, conf_path):
        urltpl = 'http://$rpcuser:$rpcpassword@${rpchost:-127.0.0.1}:$rpcport'
        cmd = ['bash', '-c', '. "%s"; echo -n %s' % (conf_path, urltpl)]
        self.url = run_cmd(cmd)

    def _exec(self, method, args):
        req = {'method': method, 'params': args, 'id': 1}
        cmd = ['curl', '-s', '-H', 'Content-Type: application/json', '-d', json.dumps(req), self.url]
        return self.load_response(run_cmd(cmd))
 
