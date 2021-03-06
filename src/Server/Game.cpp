#include "Game.h"
#include "Helper.h"

bool Game::insertGameToDB(){
	_id = _db.insertNewGame();
	return true;
}

void Game::initQuestionsFromDB(){
	_questions = _db.initQuestions(_questions_no);
}

string creat118Msg(Question* q){
	string msg;
	msg = "118" + Helper::getPaddedNumber(q->getQuestion().size(), 3);
	string* ans = q->getAnswers();
	msg += q->getQuestion();
	for (int i = 0; i < 4; i++){
		msg = msg + Helper::getPaddedNumber(ans[i].size(), 3);
		msg += ans[i];
	}
	return msg;
}

void Game::sendQuestionToAllUsers(){
	_currentTurnAnswers = 0;
	string msg = creat118Msg(_questions[_currQuestionIndex]);
	for (unsigned int i = 0; i < _players.size(); i++){
		try{
			_players[i]->send(msg);
		}
		catch(exception e){
			_players[0]->send("1180");
			cout << e.what() << endl;
		}
	}
}


Game::Game(const vector<User*>& users, int questionsNo, DataBase& db) : _db(db){
	_players = users;
	_questions_no = questionsNo;
	try{
		_id = _db.insertNewGame();
		_questions = _db.initQuestions(_questions_no);
		for (unsigned int i = 0; i < _players.size(); i++){
			_players[i]->setGame(this);
		}
	}catch(exception e){
		cout << e.what() << endl;
	}
	int i = 0;
	for (std::map<string, int>::iterator it = _results.begin(); it != _results.end(); it++){
		_results.insert(it, std::pair<string, int>(_players[i]->getUsername(), 0));
		i++;
	}
}

Game::~Game(){

	for (unsigned int i = 0; i < _questions.size(); i++){
		delete(_questions[i]);
	}
	for (unsigned int i = 0; i < _players.size(); i++){
		delete(_players[i]);
	}
}


void Game::sendFirstQuestion(){
	sendQuestionToAllUsers();
}

void Game::handleFinishGame(){
	_db.updateGameStatus(getId());
	string msg = "121" + Helper::getPaddedNumber(_players.size(), 1);
	for (int i = 0; i < _players.size(); i++){
		msg += Helper::getPaddedNumber(_players[i]->getUsername().size(), 2) + _players[i]->getUsername() + Helper::getPaddedNumber(_results[_players[i]->getUsername()], 2);
	}
	for (unsigned int i = 0; i < _players.size(); i++){ 
		try{
			_players[i]->send(msg);
			_players[i]->setGame(nullptr);
		}
		catch (exception e){
			cout << e.what() << endl;
		}
	}
}

bool Game::handleNextTurn(){
	if (_players.size() < 1){
		handleFinishGame();
		return false;
	}
	else if (_currentTurnAnswers == _players.size()){
		if (_currQuestionIndex + 1 == _questions_no){
			handleFinishGame();
			return false;
		}
		else{
			_currQuestionIndex++;
			sendQuestionToAllUsers();
		}
	}
	return true;
}

bool Game::handleAnswerFromUser(User* u, int answerNo, int time){
	_currentTurnAnswers++;
	if (answerNo == _questions[_currQuestionIndex]->getCorrectAnswerIndex()){
		_results[u->getUsername()]++;
		_db.addAnswerToPlayer(getId(), u->getUsername(), _currQuestionIndex, _questions[_currQuestionIndex]->getAnswers()[answerNo], true, time);
		u->send("1201");
	}
	else if(answerNo != 5){
		_db.addAnswerToPlayer(getId(), u->getUsername(), _currQuestionIndex, _questions[_currQuestionIndex]->getAnswers()[answerNo], false, time);
		u->send("1200");
	}
	else{
		_db.addAnswerToPlayer(getId(), u->getUsername(), _currQuestionIndex, "", false, time);
		u->send("1200");
	}
	handleNextTurn();
	if ((_questions_no == _currQuestionIndex + 1) && (_currentTurnAnswers == _players.size())){
		return false;
	}
	else{
		return true;
	}
}

bool Game::leaveGame(User* currUser){
	std::vector<User*>::iterator position = std::find(_players.begin(), _players.end(), currUser);
	if (position != _players.end()){
		_players.erase(position);
	}
	handleNextTurn();
	if ((_currentTurnAnswers == _players.size() && _currQuestionIndex + 1 == _questions_no) || _players.size() < 1){
		return false;
	}
	else{
		return true;
	}
}

int Game::getId(){
	return _id;
}
