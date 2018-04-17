@0xc8927f547a124ed2;

struct CacheArray {
	name @0: Text;
	size @1: UInt64;
	domain @2: UInt32;
	range @3: UInt32;
	constantValues @4: List(CacheExpr);
}

struct CacheAssignment {
	struct Binding {
		objects @0: CacheArray;
		bvs @1: List(UInt8);
	}
	allowFreeValues @0: Bool;
	bindings @1: List(Binding);
	noBinding @2: Bool;
}

struct CacheConstExpr {
	constExprWidth @0: UInt32;
	constExprVal @1: UInt64;
	constExprOverSizeVal @2: Text;
}

struct CacheExtractExpr {
	extractBitOff @0: UInt32;
	extractWidth @1: UInt32;
	expr @2: CacheExpr;
}

struct CacheNotOptimisedExpr {
	src @0: CacheExpr;
}

struct CacheUpdateNode {
	updateIndex @0: CacheExpr;
	updateValue @1: CacheExpr;
	next @2: CacheUpdateNode;
}

struct CacheReadExpr {
	root @0: CacheArray;
	expr @1: CacheExpr;
	head @2: CacheUpdateNode;
}

struct CacheExpr {
	width @0: UInt32;
	kind @1: UInt8;
	kids @2: List(CacheExpr);
	specialData: union {
		nothing @3: Void;
		constData @4: CacheConstExpr;
		readData @5: CacheReadExpr;
		extractData @6: CacheExtractExpr;
		nOData @7: CacheNotOptimisedExpr; 
	}
}

struct CacheExprList {
	key @0: List(CacheExpr);
}

struct CapCache {
	struct Elem {
		key @0: CacheExprList;
		assignment @1: CacheAssignment;
	}
	elems @0: List(Elem);
}

struct CacheTrieNode {
    struct Child {
        expr @0: CacheExpr;
        node @1: CacheTrieNode;
    }
    value @0: CacheAssignment;
    last @1: UInt8;
    children @2: List(Child);
}

struct CacheTrie {
    root @0: CacheTrieNode;
}
