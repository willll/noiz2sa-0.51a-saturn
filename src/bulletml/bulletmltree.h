/// BulletML�c���[�B
/**
 * BulletML �ɓ������Ă��Ĕ����ɂ��񂿂��������̂������B
 */

#ifndef BULLETMLTREE_H_
#define BULLETMLTREE_H_

#include <string>
#include <algorithm>
#include <memory>

#include <vector>

#include "tree.h"
#include "formula.h"
#include "bulletmlcommon.h"

class BulletMLNode : public TreeNode<BulletMLNode> {
public:
	typedef Formula<double> Number;
	typedef enum { none, aim, absolute, relative, sequence, typeSize } Type;
	typedef enum { bullet, action, fire, changeDirection, changeSpeed, accel,
				   wait, repeat, bulletRef, actionRef, fireRef, vanish,
				   horizontal, vertical, term, times, direction, speed, param,
				   bulletml, nameSize } Name;

private:
	static Type string2type(const std::string& str);
	static Name string2name(const std::string& str);
	static std::string name2string[nameSize];

public:
    typedef TreeNode<BulletMLNode>::Children Children;
    typedef TreeNode<BulletMLNode>::ChildIterator ChildIterator;

public:
	explicit BulletMLNode(const std::string& name);
    virtual ~BulletMLNode();

    Name getName() const { return name_; }

    void setValue(const std::string& val);
    double getValue() const { return val_->value(); }

	void setType(const std::string& type) { type_ = string2type(type); }
	Type getType() const { return type_; }

	void setRefID(int id) { refID_ = id; }
	int getRefID() const { return refID_; }

    BulletMLNode* getChild(Name name);
/*

    template <class OutIte_>
    void getAllChildren(Name name, OutIte_ outIte);
*/
    void getAllChildrenVec(Name name, std::vector<BulletMLNode*>& outvec);



    /// �q���̒��Ɏw�肵�����O�Ɉ��v�������̂����邩�ǂ���
    bool findNode(Name name) const;

    BulletMLNode* next();

    void dump();

protected:
    Name name_;
	Type type_;
	int refID_;
    std::unique_ptr<Number> val_;
};


/*
template <class OutIte_>
void BulletMLNode::getAllChildren(Name name, OutIte_ outIte) {
    ChildIterator ite;
    for (ite = childBegin(); ite != childEnd(); ite++) {
		if ((*ite)->getName() == name) *outIte = *ite;
		outIte++;
    }
}

*/

#endif // ! BULLETMLTREE_H_
