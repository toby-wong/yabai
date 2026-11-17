// Exercise missing-window recovery with synthetic windows and explicit timer
// delivery. The callback substitutes for AX registration, not the retry policy.
#define TEST_RECOVERY_PID 0x7ffffff0

static struct test_ordered *test_recovery_windows;
static int test_recovery_calls;
static int test_recovery_failures;

static void test_recovery_init(struct test_ordered *t)
{
    test_ordered_init(t);
    if (!g_window_manager.application.capacity) {
        table_init(&g_window_manager.application, 150, hash_wm, compare_wm);
    }
    memset(&g_window_recovery, 0, sizeof(g_window_recovery));
    test_recovery_windows = t;
    test_recovery_calls = 0;
    test_recovery_failures = 0;
    t->application.pid = TEST_RECOVERY_PID;

    space_manager_untile_window(t->view, &t->a);
    window_manager_remove_managed_window(&g_window_manager, t->a.id);
    window_manager_remove_window(&g_window_manager, t->a.id);
}

static enum window_recovery_result test_recovery_register(uint32_t wid, pid_t pid)
{
    ++test_recovery_calls;
    if (test_recovery_calls <= test_recovery_failures) return WINDOW_RECOVERY_RETRY;
    if (wid != test_recovery_windows->a.id || pid != TEST_RECOVERY_PID) return WINDOW_RECOVERY_RETRY;

    struct window *window = &test_recovery_windows->a;
    window_manager_add_window(&g_window_manager, window);
    struct view *view = space_manager_tile_window_on_space(&g_space_manager, window, TEST_ORDERED_SID);
    window_manager_add_managed_window(&g_window_manager, window, view);
    return WINDOW_RECOVERY_FINISHED;
}

TEST_FUNC(window_recovery_registers_missing_window,
{
    struct test_ordered t;
    test_recovery_init(&t);
    window_recovery_begin(t.a.id, t.application.pid);
    TEST_CHECK(window_manager_find_window(&g_window_manager, t.a.id) == NULL, true);

    window_recovery_run(g_window_recovery.generation, test_recovery_register);
    TEST_CHECK(test_recovery_calls, 1);
    TEST_CHECK(window_manager_find_window(&g_window_manager, t.a.id) == &t.a, true);
    TEST_CHECK(window_manager_find_managed_window(&g_window_manager, &t.a) == t.view, true);
    TEST_CHECK(view_find_window_node(t.view, t.b.id) != NULL, true);
    TEST_CHECK(g_window_recovery.wid, 0);
    test_ordered_free(&t);
});

TEST_FUNC(window_recovery_defers_to_normal_creation,
{
    struct test_ordered t;
    test_recovery_init(&t);
    window_recovery_begin(t.a.id, t.application.pid);
    uint32_t generation = g_window_recovery.generation;
    test_recovery_register(t.a.id, t.application.pid);
    test_recovery_calls = 0;

    window_recovery_run(generation, test_recovery_register);
    TEST_CHECK(test_recovery_calls, 0);
    TEST_CHECK(window_manager_find_managed_window(&g_window_manager, &t.a) == t.view, true);
    TEST_CHECK(g_window_recovery.wid, 0);
    test_ordered_free(&t);
});

TEST_FUNC(window_recovery_retries_transient_ax_failure,
{
    struct test_ordered t;
    test_recovery_init(&t);
    test_recovery_failures = 2;
    window_recovery_begin(t.a.id, t.application.pid);
    uint32_t generation = g_window_recovery.generation;
    window_recovery_run(generation, test_recovery_register);
    window_recovery_run(generation, test_recovery_register);
    TEST_CHECK(window_manager_find_window(&g_window_manager, t.a.id) == NULL, true);

    window_recovery_run(generation, test_recovery_register);
    TEST_CHECK(test_recovery_calls, 3);
    TEST_CHECK(window_manager_find_window(&g_window_manager, t.a.id) == &t.a, true);
    TEST_CHECK(g_window_recovery.wid, 0);
    test_ordered_free(&t);
});

TEST_FUNC(window_recovery_deduplicates_focus_events,
{
    struct test_ordered t;
    test_recovery_init(&t);
    test_recovery_failures = 1;
    window_recovery_begin(t.a.id, t.application.pid);
    uint32_t generation = g_window_recovery.generation;
    window_recovery_run(generation, test_recovery_register);
    window_recovery_begin(t.a.id, t.application.pid);
    TEST_CHECK(g_window_recovery.generation == generation, true);
    TEST_CHECK(g_window_recovery.attempts, 1);

    window_recovery_run(generation, test_recovery_register);
    TEST_CHECK(test_recovery_calls, 2);
    test_ordered_free(&t);
});

TEST_FUNC(window_recovery_ignores_stale_timer,
{
    struct test_ordered t;
    test_recovery_init(&t);
    window_recovery_begin(t.a.id, t.application.pid);
    uint32_t first = g_window_recovery.generation;
    window_recovery_begin(t.a.id + 10, t.application.pid);
    uint32_t second = g_window_recovery.generation;
    window_recovery_begin(t.a.id, t.application.pid);
    uint32_t current = g_window_recovery.generation;

    window_recovery_run(first, test_recovery_register);
    window_recovery_run(second, test_recovery_register);
    TEST_CHECK(test_recovery_calls, 0);
    TEST_CHECK(g_window_recovery.generation == current, true);
    window_recovery_run(current, test_recovery_register);
    TEST_CHECK(test_recovery_calls, 1);
    test_ordered_free(&t);
});

TEST_FUNC(window_recovery_bounds_retries_and_allows_later_focus,
{
    struct test_ordered t;
    test_recovery_init(&t);
    test_recovery_failures = 100;
    window_recovery_begin(t.a.id, t.application.pid);
    uint32_t generation = g_window_recovery.generation;
    for (int i = 0; i < WINDOW_RECOVERY_ATTEMPTS + 2; ++i) {
        window_recovery_run(generation, test_recovery_register);
    }
    TEST_CHECK(test_recovery_calls, WINDOW_RECOVERY_ATTEMPTS);
    TEST_CHECK(g_window_recovery.wid, 0);

    test_recovery_failures = 0;
    window_recovery_begin(t.a.id, t.application.pid);
    window_recovery_run(generation, test_recovery_register);
    TEST_CHECK(test_recovery_calls, WINDOW_RECOVERY_ATTEMPTS);
    window_recovery_run(g_window_recovery.generation, test_recovery_register);
    TEST_CHECK(window_manager_find_window(&g_window_manager, t.a.id) == &t.a, true);
    test_ordered_free(&t);
});

TEST_FUNC(window_recovery_stops_after_application_exit,
{
    struct test_ordered t;
    test_recovery_init(&t);
    window_recovery_begin(t.a.id, t.application.pid);
    EVENT_HANDLER_WINDOW_RECOVERY((void *)(uintptr_t) g_window_recovery.generation, 0);
    TEST_CHECK(g_window_recovery.wid, 0);
    TEST_CHECK(window_manager_find_window(&g_window_manager, t.a.id) == NULL, true);
    TEST_CHECK(view_find_window_node(t.view, t.b.id) != NULL, true);
    test_ordered_free(&t);
});

TEST_FUNC(window_recovery_rejects_missing_identity_and_tracked_windows,
{
    struct test_ordered t;
    test_recovery_init(&t);
    window_recovery_begin(0, t.application.pid);
    window_recovery_begin(t.a.id, 0);
    window_recovery_begin(t.b.id, t.application.pid);
    TEST_CHECK(g_window_recovery.wid, 0);
    TEST_CHECK(g_window_recovery.generation, 0);
    test_ordered_free(&t);
});
