"use strict";

const native = require("../build/Release/njs-test.node");

// ============================================================================
// [Boilerplate]
// ============================================================================

function log(text) {
  console.log("  [JS~] " + text);
}

function test(description, fn) {
  console.log(`${description}:`);

  var complete = false;
  function done() {
    complete = true;
    console.log("");
  }

  try {
    fn(done);
  }
  catch (ex) {
    log(`FAILURE: ${description}: ${ex.toString()}`);
    throw ex;
  }

  if (complete === false) {
    // TODO: Async testing.
  }
}

function assertEqual(a, b) {
  if (a === b) return;
  throw new TypeError(`Expected ${String(a)} === ${String(b)}`);
}

function assertThrow(fn) {
  try {
    fn();
  } catch (ex) {
    log(`Thrown as expected: ${ex.toString()}`);
    return;
  }
  throw new TypeError(`[JS] FAILURE: Expected an exception to be thrown`);
}

// ============================================================================
// [Basic - Instantiate 'native.Object' and use get/set, methods, and statics]
// ============================================================================

test("Basic functionality", function(done) {
  var NObj = native.Object;
  var inst = new NObj(1, 2);

  assertEqual(inst.a, 1);
  assertEqual(inst.b, 2);

  inst.a = 42;
  assertEqual(inst.a, 42);

  inst.add(1);
  assertEqual(inst.a, 43);
  assertEqual(inst.b, 3);

  // Should throw if `NObj` was called without using the new operator.
  assertThrow(function() { NObj(1, 2); });

  // Should throw if an invalid number (not int32) is passed to getter / method.
  assertThrow(function() { inst.a = NaN; });
  assertThrow(function() { inst.a = Infinity; });
  assertThrow(function() { inst.a = Math.pow(2, 32); });

  assertThrow(function() { inst.add(NaN); });
  assertThrow(function() { inst.add(Infinity); });
  assertThrow(function() { inst.add(Math.pow(2, 32)); });

  // Should throw if an invalid type is passed to getter of method.
  assertThrow(function() { inst.a = "42"; });
  assertThrow(function() { inst.a = "string"; });
  assertThrow(function() { inst.a = {}; });
  assertThrow(function() { inst.a = []; });

  assertThrow(function() { inst.add("42"); });
  assertThrow(function() { inst.add("string"); });
  assertThrow(function() { inst.add({}); });
  assertThrow(function() { inst.add([]); });

  // Should throw as `inst` has not a setter for `b`.
  assertThrow(function() { inst.b = 1; });

  // Should call `NObj.staticMul()` (static method).
  assertEqual(NObj.staticMul(21, 2), 42);
  assertEqual(inst.staticMul, undefined);

  done();
});

// ============================================================================
// [Basic - Instantiate 'native.Object' and tests if bindings are secure]
// ============================================================================

test("Basic security (incompatible signature)", function(done) {
  var NObj = native.Object;

  var instA = new NObj(1, 2);
  var instB = new NObj(2, 3);
  var instC = new NObj(1, 2);
  var fake = {};

  assertEqual(instA.equals(instB), false);
  assertEqual(instA.equals(instC), true);
  assertThrow(function() { instA.equals(fake); });

  fake.add = instA.add;
  assertThrow(function() { fake.add(1, 2); });

  fake = { __proto__: instA };
  assertThrow(function() { fake.add(1, 2); }); // Method.
  assertThrow(function() { fake.a; });         // Getter.
  assertThrow(function() { fake.a = 1; });     // Setter.
  assertThrow(function() { fake.b = 1; });     // Setter (read-only).

  fake = { __proto__: NObj.prototype };
  assertThrow(function() { fake.add(1, 2); }); // Method.

  done();
});

console.log("All tests passed!");
