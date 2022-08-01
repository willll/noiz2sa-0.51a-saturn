#ifndef BULLETMLERROR_H_
#define BULLETMLERROR_H_

#include "bulletmlcommon.h"

#include <stdexcept>

/// �������ɂ����������Ə[���������Ƃ˂�
class BulletMLError  {
public:
 BulletMLError(const std::string& msg)
 {}

	static void doAssert(const char* str) {

	}
	static void doAssert(const std::string& str) {

	}
	static void doAssert(bool t, const char* str) {

	}
	static void doAssert(bool t, const std::string& str) {
		//if (!t) throw BulletMLError(str);
	}

};

#endif // ! BULLETMLERROR_H_
