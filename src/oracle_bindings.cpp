
#include "oracle_bindings.h"
#include "connection.h"
#include "outParam.h"

Persistent<FunctionTemplate> OracleClient::s_ct;

struct connect_baton_t {
  OracleClient* client;
  Persistent<Function> callback;
  std::string hostname;
  std::string user;
  std::string password;
  std::string database;
  std::string tns;
  uint32_t port;
  oracle::occi::Environment *environment;

  std::string* error;
  oracle::occi::Connection* connection;
};

void OracleClient::Init(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  s_ct = Persistent<FunctionTemplate>::New(t);
  s_ct->InstanceTemplate()->SetInternalFieldCount(1);
  s_ct->SetClassName(String::NewSymbol("OracleClient"));

  NODE_SET_PROTOTYPE_METHOD(s_ct, "connect", Connect);
  NODE_SET_PROTOTYPE_METHOD(s_ct, "connectSync", ConnectSync);

  target->Set(String::NewSymbol("OracleClient"), s_ct->GetFunction());
}

Handle<Value> OracleClient::New(const Arguments& args) {
  HandleScope scope;

  /*
  REQ_OBJECT_ARG(0, settings);

  std:string hostname, user, password, database;
  unsigned int port, minConn, maxConn, incrConn;

  OBJ_GET_STRING(settings, "hostname", hostname);
  OBJ_GET_STRING(settings, "user", user);
  OBJ_GET_STRING(settings, "password", password);
  OBJ_GET_STRING(settings, "database", database);
  OBJ_GET_NUMBER(settings, "port", port, 1521);
  OBJ_GET_NUMBER(settings, "minConn", minConn, 0);
  OBJ_GET_NUMBER(settings, "maxConn", maxConn, 1);
  OBJ_GET_NUMBER(settings, "incrConn", incrConn, 1);

  std::ostringstream connectionStr;
      connectionStr << "//" << hostname << ":" << port << "/" << database;
  */

  OracleClient *client = new OracleClient();
  client->Wrap(args.This());
  return args.This();
}

OracleClient::OracleClient() {
  m_environment = oracle::occi::Environment::createEnvironment(oracle::occi::Environment::THREADED_MUTEXED);
  /*
  m_connectionPool = m_environment->createConnectionPool(user, password, connectString, minConn, maxConn, incrConn);
  */
}

OracleClient::~OracleClient() {
  /*
  m_environment->terminateConnectionPool (connectionPool);
  */
  oracle::occi::Environment::terminateEnvironment(m_environment);
}

Handle<Value> OracleClient::Connect(const Arguments& args) {
  HandleScope scope;

  REQ_OBJECT_ARG(0, settings);
  REQ_FUN_ARG(1, callback);

  OracleClient* client = ObjectWrap::Unwrap<OracleClient>(args.This());
  connect_baton_t* baton = new connect_baton_t();
  baton->client = client;
  baton->callback = Persistent<Function>::New(callback);
  baton->environment = client->m_environment;
  baton->error = NULL;
  baton->connection = NULL;

  baton->hostname = "127.0.0.1";
  baton->user = "";
  baton->password = "";
  baton->database = "";

  OBJ_GET_STRING(settings, "hostname", baton->hostname);
  OBJ_GET_STRING(settings, "user", baton->user);
  OBJ_GET_STRING(settings, "password", baton->password);
  OBJ_GET_STRING(settings, "database", baton->database);
  OBJ_GET_NUMBER(settings, "port", baton->port, 1521);
  OBJ_GET_STRING(settings, "tns", baton->tns);

  client->Ref();

  uv_work_t* req = new uv_work_t();
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, EIO_Connect, (uv_after_work_cb)EIO_AfterConnect);

  return scope.Close(Undefined());
}

void OracleClient::EIO_Connect(uv_work_t* req) {
  connect_baton_t* baton = static_cast<connect_baton_t*>(req->data);

  baton->error = NULL;

  try {
    std::ostringstream connectionStr;
    if (baton->tns != "") {
      connectionStr << baton->tns;
    } else {
      connectionStr << "//" << baton->hostname << ":" << baton->port << "/" << baton->database;
    }
    baton->connection = baton->environment->createConnection(baton->user, baton->password, connectionStr.str());
  } catch(oracle::occi::SQLException &ex) {
    baton->error = new std::string(ex.getMessage());
  }
}

void OracleClient::EIO_AfterConnect(uv_work_t* req, int status) {
  HandleScope scope;
  connect_baton_t* baton = static_cast<connect_baton_t*>(req->data);
  baton->client->Unref();

  Handle<Value> argv[2];
  if(baton->error) {
    argv[0] = Exception::Error(String::New(baton->error->c_str()));
    argv[1] = Undefined();
  } else {
    argv[0] = Undefined();
    Handle<Object> connection = Connection::constructorTemplate->GetFunction()->NewInstance();
    (node::ObjectWrap::Unwrap<Connection>(connection))->setConnection(baton->client->m_environment, baton->connection);
    argv[1] = connection;
  }

  v8::TryCatch tryCatch;
  baton->callback->Call(Context::GetCurrent()->Global(), 2, argv);
  if (tryCatch.HasCaught()) {
    node::FatalException(tryCatch);
  }

  baton->callback.Dispose();
  if(baton->error) delete baton->error;

  delete baton;
}

Handle<Value> OracleClient::ConnectSync(const Arguments& args) {
  HandleScope scope;
  REQ_OBJECT_ARG(0, settings);

  OracleClient* client = ObjectWrap::Unwrap<OracleClient>(args.This());
  connect_baton_t baton;
  baton.client = client;
  baton.environment = client->m_environment;
  baton.error = NULL;
  baton.connection = NULL;

  baton.hostname = "127.0.0.1";
  baton.user = "";
  baton.password = "";
  baton.database = "";

  OBJ_GET_STRING(settings, "hostname", baton.hostname);
  OBJ_GET_STRING(settings, "user", baton.user);
  OBJ_GET_STRING(settings, "password", baton.password);
  OBJ_GET_STRING(settings, "database", baton.database);
  OBJ_GET_NUMBER(settings, "port", baton.port, 1521);

  try {
    std::ostringstream connectionStr;
    connectionStr << "//" << baton.hostname << ":" << baton.port << "/" << baton.database;
    baton.connection = baton.environment->createConnection(baton.user, baton.password, connectionStr.str());
  } catch(oracle::occi::SQLException &ex) {
    baton.error = new std::string(ex.getMessage());
    return scope.Close(ThrowException(Exception::Error(String::New(baton.error->c_str()))));
  } catch (const std::exception& ex) {
    return scope.Close(ThrowException(Exception::Error(String::New(ex.what()))));
  }

  Handle<Object> connection = Connection::constructorTemplate->GetFunction()->NewInstance();
      (node::ObjectWrap::Unwrap<Connection>(connection))->setConnection(baton.client->m_environment, baton.connection);

  return scope.Close(connection);

}


extern "C" {
  static void init(Handle<Object> target) {
    OracleClient::Init(target);
    Connection::Init(target);
    OutParam::Init(target);
  }

  NODE_MODULE(oracle_bindings, init);
}
