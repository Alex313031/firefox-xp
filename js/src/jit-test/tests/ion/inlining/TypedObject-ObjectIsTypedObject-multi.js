// |jit-test| skip-if: !this.TypedObject
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Used to verify that the JIT resolves the ObjectIsTypedObject tests
 * used in the TO.objectType() method, among other places.
 *
 * In this case the argument type is always a TypedObject (though not
 * a unique one), so ObjectIsTypedObject resolves to true and there
 * should be no exceptions.
 *
 * Load this into the js shell with IONFLAGS=logs, then exit and run
 * iongraph.  You're looking for a smallish function within the
 * "self-hosted" domain.  Look for a call to ObjectIsTypedObject far
 * down in the graph for pass00, with a subgraph before it that looks
 * like it's comparing something to a string and to null (this is the
 * inlining of IsObject).  (All of this is at the mercy of the way the
 * code is currently written.)
 */

var T = TypedObject;
var ST1 = new T.StructType({x:T.int32});
var ST2 = new T.StructType({f:T.float64});
var v1 = new ST1({x:10});
var v2 = new ST2({f:3.14159});

function check(v) {
  return T.objectType(v);
}

function test() {
  var a = [ v1, v2 ];
  for ( var i=0 ; i < 1000 ; i++ )
    assertEq(check(a[i%2]), ((i%2)==0 ? ST1 : ST2));
  return check(a[i%2]);
}

print("Done");
