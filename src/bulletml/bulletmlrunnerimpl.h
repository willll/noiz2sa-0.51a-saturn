#ifndef BULLETRUNNER_IMPL_H_
#define BULLETRUNNER_IMPL_H_

#include "bulletmltree.h"

#include <vector>
#include <memory>
#include <stack>

class BulletMLRunner;
class BulletMLState;
class BulletMLParser;

typedef std::vector<double> BulletMLParameter;

template<class C_>
class Validatable {
public:
	Validatable() : isValidate_(false) {}

	bool isValidate() const { return isValidate_; }

	void enValidate() { isValidate_ = true; }
	void disValidate() { isValidate_ = false; }

	operator C_& () { return val_; }

	C_& operator = (const C_& rhs) {
		isValidate_ = true;
		val_ = rhs;
		return *this;
	}

protected:
	C_ val_;

	bool isValidate_;
};

/// xy�̏����l�E�I�l�����C�ӂ�x�ɑ΂���y�̐��`���Ԃ𓾂��N���X
template <class X_ = double, class Y_ = double>
class LinearFunc {
public:
	LinearFunc(const X_& firstX, const X_& lastX,
			   const Y_& firstY, const Y_& lastY)
		: firstX_(firstX), lastX_(lastX),
		  firstY_(firstY), lastY_(lastY),
		  gradient_((lastY-firstY)/(lastX-firstX)) {}

	Y_ getValue(const X_& x) {
		return firstY_ + gradient_ * (x-firstX_);
	}

	bool isLast(const X_& x) {
		return x >= lastX_;
	}
	Y_ getLast() {
		return lastY_;
	}

protected:
	X_ firstX_, lastX_;
	Y_ firstY_, lastY_;
	Y_ gradient_;
};

class BulletMLRunnerImpl {
public:
    explicit BulletMLRunnerImpl(BulletMLState* state, BulletMLRunner* runner);
    virtual ~BulletMLRunnerImpl();

	/// ���s����
    void run();

public:
	/// ���s���I�����Ă��邩�ǂ���
	bool isEnd() const {
		return end_;
	}

public:
    /// �e�̕����ύX���o�^���A���O�Ŋe�^�[���ύX����
	virtual void calcChangeDirection(double direction, int term, bool seq);
    /// �e�̑��x�ύX���o�^���A���O�Ŋe�^�[���ύX����
	virtual void calcChangeSpeed(double speed, int term);
    /// �e�̉������o�^���A���O�Ŋe�^�[���ύX����
	/**
	 * @todo horizontal, vertical �� type �͖������ł��B
	 */
	virtual void calcAccelX(double vertical, int term,
							BulletMLNode::Type type);
    /// �e�̉������o�^���A���O�Ŋe�^�[���ύX����
	/**
	 * @todo horizontal, vertical �� type �͖������ł��B
	 */
	virtual void calcAccelY(double horizontal, int term,
							BulletMLNode::Type type);

protected:
	/**
	 * �{���ɋ������C�ɓ����Ȃ��ꍇ�͉��z�֐������āA
	 * �������̃I�[�o�[���C�h���l���Ă��������B
	 */
	//@{
    void runBullet();
    void runAction();
    void runFire();
    void runWait();
    void runRepeat();
    void runBulletRef();
    void runActionRef();
    void runFireRef();
    void runChangeDirection();
    void runChangeSpeed();
    void runAccel();
    void runVanish();
	//@}

private:
	void changes();
	void runSub();
	void init();

	bool isTurnEnd();
	void doWait(int frame);

    void setDirection();
    void setSpeed();

    void shotInit() {
		spd_.disValidate();
		dir_.disValidate();
    }

    double getNumberContents(const BulletMLNode* node);
    std::vector<double>* getParameters();
    double getSpeed(BulletMLNode* spdNode);
    double getDirection(BulletMLNode* dirNode, bool prevChange = true);

private:
private:
    std::unique_ptr<LinearFunc<int, double> > changeDir_;
    std::unique_ptr<LinearFunc<int, double> > changeSpeed_;
    std::unique_ptr<LinearFunc<int, double> > accelx_;
    std::unique_ptr<LinearFunc<int, double> > accely_;

protected:
    Validatable<double> spd_, dir_, prevSpd_, prevDir_;

    typedef BulletMLParameter Parameters;
    std::shared_ptr<Parameters> parameters_;

protected:
    BulletMLParser* bulletml_;
    BulletMLNode* act_;
	std::vector<BulletMLNode*> node_;
	int actTurn_;
	std::vector<int> actTurns_;
	int endTurn_;
	size_t actIte_;
	bool end_;

protected:
	struct RepeatElem {
		RepeatElem(int i, int e, BulletMLNode* a)
			: ite(i), end(e), act(a) {}
		int ite, end;
		BulletMLNode* act;
	};
    typedef std::stack<RepeatElem*> RepeatStack;
    RepeatStack repeatStack_;
    typedef std::stack<std::pair<BulletMLNode*,
    std::shared_ptr<Parameters> > > RefStack;
    RefStack refStack_;

    typedef void (BulletMLRunnerImpl::*Method)();
	static Method commandMap_[BulletMLNode::nameSize];

protected:
	BulletMLRunner* runner_;

};

#endif // ! BULLETRUNNER_IMPL_H_
