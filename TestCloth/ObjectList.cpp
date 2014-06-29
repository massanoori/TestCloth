#include "stdafx.h"
#include "ObjectList.h"

#include <unordered_set>

void Object::Update()
{
	UpdateImpl();
}

void Object::Render() const
{
	RenderImpl();
}

class ObjectList::Impl
{
public:
	void Update()
	{
		for (auto& object : m_Objects)
		{
			object->Update();
		}
	}

	void Render() const
	{
		for (const auto& object : m_Objects)
		{
			object->Render();
		}
	}

	void AddObject(ObjectHandle object)
	{
		m_Objects.insert(object);
	}

	void RemoveObject(ObjectHandle object)
	{
		auto itr = m_Objects.find(object);
		if (itr != m_Objects.end())
		{
			m_Objects.erase(itr);
		}
	}

private:
	// object container
	std::unordered_set<ObjectHandle> m_Objects;
};

ObjectList::~ObjectList()
{
	delete m_pImpl;
}

void ObjectList::Initialize()
{
	delete m_pImpl;
	m_pImpl = new Impl;
}

void ObjectList::Update()
{
	m_pImpl->Update();
}

void ObjectList::Render() const
{
	m_pImpl->Render();
}

void ObjectList::AddObject(ObjectHandle object)
{
	m_pImpl->AddObject(object);
}

void ObjectList::RemoveObject(ObjectHandle object)
{
	m_pImpl->RemoveObject(object);
}
