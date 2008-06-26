#include "types.h"

#include "code.h"
#include "interpreter.h"
#include "tree.h"
#include "debug.h"

#include <cassert>


void Value::dump(std::ostream &stream) const
{
	stream << unmangle(typeid(*this).name()) << " ";
	dumpSpecific(stream);
}


Prototype Nil(0);
Value nil(0, &Nil);


ThunkPrototype::ThunkPrototype(Heap* heap, Prototype* outer):
	Prototype(heap),
	outer(outer)
{}


struct ScopeSize: NativeThunk
{
	ScopeSize():
		NativeThunk(0, "Scope::size")
	{}
	
	Value* execute(Thread* thread, Value* receiver)
	{
		Scope* scope = dynamic_cast<Scope*>(receiver);
		assert(scope);
		return new Integer(thread->heap, scope->locals.size());
	}
} scopeSize;

ScopePrototype::ScopePrototype(Heap* heap, Prototype* outer):
	ThunkPrototype(heap, outer)
{
	members["size"] = &scopeSize;
}

ScopePrototype::~ScopePrototype()
{
	for (Body::iterator it = body.begin(); it != body.end(); ++it)
		delete *it;
}


Scope::Scope(Heap* heap, ScopePrototype* prototype, Value* outer):
	Thunk(heap, prototype, outer),
	locals(prototype->locals.size(), 0)
{}
	

Method::Method(Heap* heap, Prototype* outer):
ScopePrototype(heap, outer)
{}


NativeThunk::NativeThunk(Prototype* outer, const std::string& name):
	ThunkPrototype(0, outer),
	name(name)
{
	body.push_back(new ThunkCode());
	body.push_back(new ParentCode());
	body.push_back(new NativeThunkCode(this));
}


NativeMethod::NativeMethod(Prototype* outer, const std::string& name, PatternNode* argument):
	Method(0, outer),
	name(name)
{
	argument->generate(this, 0, (Heap*) 0);
	delete argument;
	body.push_back(new ThunkCode());
	body.push_back(new ParentCode());
	body.push_back(new ThunkCode());
	body.push_back(new ValRefCode(0));
	body.push_back(new NativeMethodCode(this));
}


Function::Function(Heap* heap, Method* prototype, Value* outer):
	Scope(heap, prototype, outer)
{}


struct IntegerAdd: NativeMethod
{
	IntegerAdd():
		NativeMethod(&Integer::integerPrototype, "Integer::+", new ValPatternNode(Position(), "that"))
	{}
	
	Value* execute(Thread* thread, Value* receiver, Value* argument)
	{
		Integer* thisInt = dynamic_cast<Integer*>(receiver);
		Integer* thatInt = dynamic_cast<Integer*>(argument);
		
		assert(thisInt);
		assert(thatInt);
		
		return new Integer(thread->heap, thisInt->value + thatInt->value);
	}
} integerAdd;

struct IntegerSub: NativeMethod
{
	IntegerSub():
		NativeMethod(&Integer::integerPrototype, "Integer::-", new ValPatternNode(Position(), "that"))
	{}
	
	Value* execute(Thread* thread, Value* receiver, Value* argument)
	{
		Integer* thisInt = dynamic_cast<Integer*>(receiver);
		Integer* thatInt = dynamic_cast<Integer*>(argument);
		
		assert(thisInt);
		assert(thatInt);
		
		return new Integer(thread->heap, thisInt->value - thatInt->value);
	}
} integerSub;

Integer::IntegerPrototype::IntegerPrototype():
	Prototype(0)
{
	members["+"] = nativeMethodMember(&integerAdd);
	members["-"] = nativeMethodMember(&integerSub);
}

Integer::IntegerPrototype Integer::integerPrototype;

