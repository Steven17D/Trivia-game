#include "TriviaServer.h"

TriviaServer::TriviaServer() : _ul(_mtxRecievedMessages){
	//db constrastor is automatically called and he will throw an exception if failed
	//need to create new socket
	_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_socket == INVALID_SOCKET)
	{
		throw exception("Faild to create a socket."); //Couldn't create the socket
	}
}
TriviaServer::~TriviaServer(){
	for (std::map<int, Room*>::iterator it = _roomList.begin(); it != _roomList.end(); ++it){
		delete it->second;
	}
	for (std::map<SOCKET, User*>::iterator it = _connectedUsers.begin(); it != _connectedUsers.end(); ++it){
		delete it->second;
	}
	closesocket(_socket);
}

void TriviaServer::server(){
	bindAndListen();
	//thread t(this->clientHandler, _socket);
	//t.join();
	while (true){
		this->accept();
	}
}
void TriviaServer::bindAndListen(){
	SOCKADDR_IN addr; // The address structure for a TCP socket
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != NO_ERROR) {
		throw exception("WSAStartup failed with error: " + iResult);
	}
	addr.sin_family = AF_INET;      // Address family
	addr.sin_port = htons(5656);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (::bind(_socket, (LPSOCKADDR)&addr, sizeof(addr)) == SOCKET_ERROR){
		//We couldn't bind (this will happen if you try to bind to the same  
		//socket more than once)
		throw exception("Failed to bind");
	}
	if (listen(_socket, SOMAXCONN) < 0){
		throw exception("Failed to listen");
	}
}
void TriviaServer::accept(){
	SOCKET acceptSock;
	acceptSock = ::accept(_socket, NULL, NULL);
	thread clientT(&TriviaServer::clientHandler, TriviaServer(), acceptSock);
	clientT.join();
}
void TriviaServer::clientHandler(SOCKET s){
	try{
		int messageTypeCode = Helper::getMessageTypeCode(s);
		if (messageTypeCode != 0 && messageTypeCode != 299){ //???????
			addRecievedMessage(buildRecieveMessage(s, 0/*???????*/));
			messageTypeCode = Helper::getMessageTypeCode(s);
		}
		addRecievedMessage(buildRecieveMessage(s, 299));
	}
	catch (exception e){
		addRecievedMessage(buildRecieveMessage(s, 299));
	}
}
void TriviaServer::safeDeleteUser(RecievedMessage* r){
	try{
		SOCKET client_socket = r->getSock();
		handleSignout(r);
		closesocket(client_socket);
	}
	catch (exception e){
		cout << e.what() << endl;
	}
}

User* TriviaServer::handleSignin(RecievedMessage* r){
	if (_db.isUserAndPassMatch(r->getValues()[0], r->getValues()[1])){
		if (getUserByName(r->getUser()->getUsername()) == nullptr){
			r->getUser()->send("1022");
			return nullptr;
		}
		else{
			pair<SOCKET, User*> u(r->getSock(), r->getUser());
			_connectedUsers.insert(u);
			r->getUser()->send("1020");
			return r->getUser();
		}
	}
	else{
		r->getUser()->send("1021");
		return nullptr;
	}
}
bool TriviaServer::handleSignup(RecievedMessage* r){
	if (Validator::isPasswordValid(r->getValues()[1])){
		if (Validator::isUsernameValid(r->getValues()[0])){
			if (!_db.isUserExists(r->getValues()[0])){
				if (!_db.addNewUser(r->getValues()[0], r->getValues()[1], r->getValues()[2])){
					r->getUser()->send("1044");
				}
				else{
					r->getUser()->send("1040");
					return true;
				}
			}
			else{
				r->getUser()->send("1042");
			}
		}
		else{
			r->getUser()->send("1043");
		}
	}
	else{
		r->getUser()->send("1041");
	}
	return false;
}
void TriviaServer::handleSignout(RecievedMessage* r){
	bool user_exsist = false;
	if (r->getUser() != nullptr){
		std::map<SOCKET, User*>::iterator it;
		it = _connectedUsers.find(r->getSock());
		_connectedUsers.erase(it);
		this->handleCloseRoom(r);
		this->handleLeaveRoom(r);
		this->handleLeaveGame(r);
	}
}

void TriviaServer::handleLeaveGame(RecievedMessage* r){
	if (r->getUser()->leaveGame()){
		delete(r->getUser()->getGame());
	}
}
void TriviaServer::handleStartGame(RecievedMessage* r){
	vector<User*> users = r->getUser()->getRoom()->getUsers(); //users int the room
	//r->getUser()->getRoom()->
}
void TriviaServer::handlePlayerAnswer(RecievedMessage* r){
	if (r->getUser()->getGame() != nullptr){
		if (!r->getUser()->getGame()->handleAnswerFromUser(r->getUser(), atoi(r->getValues()[0].c_str()), atoi(r->getValues()[1].c_str()))){
			delete(r->getUser()->getGame());
		}
	}
}

bool TriviaServer::handleCreateRoom(RecievedMessage* r){
	if (r->getUser() != nullptr){
		_roomIdSequence++;
		r->getUser()->createRoom(_roomIdSequence, r->getValues()[0], atoi(r->getValues()[1].c_str()), atoi(r->getValues()[2].c_str()), atoi(r->getValues()[3].c_str()));
		pair<int, Room*> p(_roomIdSequence, r->getUser()->getRoom());
		_roomList.insert(p);
		return true;
	}
	return false;
}
bool TriviaServer::handleCloseRoom(RecievedMessage* r){
	if (r->getUser()->getRoom() != nullptr){
		r->getUser()->closeRoom();
		for (std::map<int, Room*>::iterator it = _roomList.begin(); it != _roomList.end(); ++it){
			if (it->second == r->getUser()->getRoom()){
				_roomList.erase(it);
			}
		}
		return true;
	}
	else{
		return false;
	}
}
bool TriviaServer::handleJoinRoom(RecievedMessage* r){
	if (r->getUser() == nullptr){
		return false;
	}
	else{
		Room* temp = getRoomById(atoi(r->getValues()[0].c_str()));
		if (temp != nullptr){
			r->getUser()->joinRoom(temp);
			return true;
		}
		else{
			r->getUser()->send("1101");
			return false;
		}
	}
}
bool TriviaServer::handleLeaveRoom(RecievedMessage* r){
	if (r->getUser() == nullptr){
		return false;
	}
	else{
		Room* temp = r->getUser()->getRoom();
		if (temp != nullptr){
			r->getUser()->leaveRoom();
			return true;
		}
		else{
			return false;
		}
	}
}
void TriviaServer::handleGetUsersInRoom(RecievedMessage* r){
	Room* temp = getRoomById(atoi(r->getValues()[0].c_str()));
	if (temp == nullptr){
		r->getUser()->send("1080");
	}
	else{
		r->getUser()->send(temp->getUsersListMessage());
	}
}
void TriviaServer::handleGetRooms(RecievedMessage* r){
	Room* result = nullptr;
	string msg = "205" + Helper::getPaddedNumber(_roomList.size(), 4);
	for (std::map<int, Room*>::iterator it = _roomList.begin(); it != _roomList.end(); ++it){
		msg += Helper::getPaddedNumber(it->first, 4) + Helper::getPaddedNumber(it->second->getName().size(), 2) + it->second->getName();
	}
	r->getUser()->send(msg);
}

void TriviaServer::handleGetBestScores(RecievedMessage* r){
	vector<string> scores = _db.getBestScores();
	string msg = "124" + scores[0]; //???????????????????
}
void TriviaServer::handleGetPersonalStatus(RecievedMessage* r){

}

void TriviaServer::handleRecievedMessage(){
	_cv.wait(_ul);
	_ul.lock();
	RecievedMessage* temp = _queRcvMessages.back();
	_ul.unlock();//????????????????????????????
	User* u = getUserBySocket(temp->getSock());
	temp->setUser(u);
	switch (temp->getMessageCode()){
	case 200:
		handleSignin(temp);
		break;
	case 201:
		handleSignout(temp);
		break;
	default:
		break;
	}
}
void TriviaServer::addRecievedMessage(RecievedMessage* r){
	_mtxRecievedMessages.lock();
	_queRcvMessages.push(r);
	_mtxRecievedMessages.unlock();
	_cv.notify_all();
}
RecievedMessage* TriviaServer::buildRecieveMessage(SOCKET s, int msgCode){
	vector <string> vals;
	string temp;
	string val = "";
	try{
		temp = Helper::getStringPartFromSocket(s, 100);
	}
	catch (exception e){
		cout << e.what() << endl;
	}
	int size;
	switch (msgCode){
	case 200:
		size = atoi(temp.substr(3, 2).c_str());
		val = temp.substr(5, size);
		vals.push_back(val);
		size = atoi(temp.substr(size + 5, 2).c_str());
		val = temp.substr(5 + val.size() + 2, size);
		vals.push_back(val);
		break;
	case 201:
		return new RecievedMessage(s, msgCode);
	default:
		break;
	}
	return new RecievedMessage(s, msgCode, vals);
}

User* TriviaServer::getUserByName(string name){
	User* result = nullptr;
	for (std::map<SOCKET, User*>::iterator it = _connectedUsers.begin(); it != _connectedUsers.end(); ++it){
		if (it->second->getUsername() == name){
			result = it->second;
		}
	}
	return result;
}
User* TriviaServer::getUserBySocket(SOCKET s){
	User* result = nullptr;
	for (std::map<SOCKET, User*>::iterator it = _connectedUsers.begin(); it != _connectedUsers.end(); ++it){
		if (it->first == s){
			result = it->second;
		}
	}
	return result;
}
Room* TriviaServer::getRoomById(int id){
	Room* result = nullptr;
	for (std::map<int, Room*>::iterator it = _roomList.begin(); it != _roomList.end(); ++it){
		if (it->first == id){
			result = it->second;
		}
	}
	return result;
}