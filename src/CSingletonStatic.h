//
// Created by qlist on 2019/1/20.
//

#pragma once

// main函数执行时，即已经被创建起来

template <class T>
class CSingletonStatic {

public:

    ~CSingletonStatic()= default;

	CSingletonStatic()= default;

	static T& getInst(){
		static T singleton;
		return singleton;
	}

private:

};
