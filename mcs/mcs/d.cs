using System.Collections;

class X {
	static void Main ()
	{
		Foo x = new Foo () { {1,2}};
	}

	class Foo :IEnumerable { public IEnumerator GetEnumerator () { return null; } }
}

