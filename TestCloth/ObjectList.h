#pragma once

#include <string>
#include <memory>

class Object
{
public:
	virtual ~Object() {}

	// update object by one frame
	void Update();

	// render current frame
	void Render() const;

protected:
	// implementation of Update(), which is overridden in subclass
	virtual void UpdateImpl() {}

	// implementation of Render(), which is overridden in subclass
	virtual void RenderImpl() const {}
};

// ObjectHandle for Object
typedef std::shared_ptr<Object> ObjectHandle;

// make object handle whose type is ObjectType
template <typename ObjectType, typename ... VArg>
ObjectHandle MakeObjectHandle(VArg ... arg)
{
	return ObjectHandle(new ObjectType(arg...));
}

// object list
class ObjectList
{
public:
	ObjectList() = default;
	ObjectList(const ObjectList&) = delete;
	~ObjectList();

	void Initialize();

	// call Update() of all the added objects
	void Update();

	// call Render() of all the added objects
	void Render() const;

	// add object to this object list
	void AddObject(ObjectHandle object);

	// remove object from this object list
	void RemoveObject(ObjectHandle object);

private:
	class Impl;
	Impl* m_pImpl = nullptr;
};
