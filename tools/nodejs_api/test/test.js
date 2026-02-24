const { describe, it, before, after } = require("node:test");
global.describe = describe;
global.it = it;
global.before = before;
global.after = after;

require("./common.js");

const path = require("path");
const fs = require("fs/promises");

// Shared init/teardown so each top-level suite can use them. initTests() runs once.
let initPromise = null;
const sharedBefore = () => {
  if (!initPromise) initPromise = initTests();
  return initPromise;
};
const sharedAfter = async () => {
  if (global.conn && !global.conn._isClosed) {
    await global.conn.close().catch(() => {});
  }
  if (global.db && !global.db._isClosed) {
    await global.db.close().catch(() => {});
  }
  if (global.tmpPath) {
    await fs.rm(global.tmpPath, { recursive: true }).catch(() => {});
  }
};

// Node's test runner only runs tests under a describe that has at least one it().
// One root describe + it("(setup)") + require() of suite files so nested suites run.
describe("lbug", () => {
  before(() => initTests());
  after(async () => {
    await sharedAfter();
    // Defer exit so the runner finishes all nested suites; process.exit(0) here directly
    // would prevent nested describe() suites from running (node:test runner quirk).
    setImmediate(() => process.exit(0));
  });

  require(path.join(__dirname, "test_database.js"));
  require(path.join(__dirname, "test_connection.js"));
  require(path.join(__dirname, "test_query_result.js"));
  require(path.join(__dirname, "test_data_type.js"));
  require(path.join(__dirname, "test_parameter.js"));
  require(path.join(__dirname, "test_concurrency.js"));
  require(path.join(__dirname, "test_version.js"));
  require(path.join(__dirname, "test_sync_api.js"));
  require(path.join(__dirname, "test_register_stream.js"));
  require(path.join(__dirname, "test_resilience.js"));
  require(path.join(__dirname, "test_call_subquery.js"));

  it("(setup)", () => {});
});
