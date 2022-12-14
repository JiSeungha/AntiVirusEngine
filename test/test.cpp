// test.cpp: 콘솔 응용 프로그램의 진입점을 정의합니다.
//

#include "stdafx.h"
#include <map>
#include <mutex>

using namespace std;

mutex m;

void test(int a) {
	for (int i = 0; i < 100; i++) {
		m.lock();
		cout << a <<" : " << i << endl;
		m.unlock();
	}
}

int main(){
	thread *ts[8];

	for (int i = 0; i < 8; i++) {
		ts[i] = new thread(&test, i);
	} 

	string str;
	cin >> str;

    return 0;
}

