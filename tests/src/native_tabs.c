// Adapted from Christian-SC26/yabai's native-tab regression cases, with additional
// checks for the hide-on-close patch. Only synthetic window and Space IDs are used.

TEST_FUNC(native_tab_preserves_bsp_node,
{
    struct window_node root = {0};
    struct window_node left = {0};
    struct window_node right = {0};
    struct view view = {0};
    root.left = &left;
    root.right = &right;
    root.split = SPLIT_Y;
    root.ratio = 0.37f;
    left.parent = right.parent = &root;
    left.window_count = right.window_count = 1;
    left.window_list[0] = left.window_order[0] = TEST_ORDERED_WID_A;
    right.window_list[0] = right.window_order[0] = TEST_ORDERED_WID_B;
    left.zoom = &root;
    view.root = &root;
    view.layout = VIEW_BSP;
    view.insertion_point = TEST_ORDERED_WID_A;

    bool swapped = view_swap_node_window(&view, TEST_ORDERED_WID_A, TEST_ORDERED_WID_A + 10);
    TEST_CHECK(swapped, true);
    TEST_CHECK(view_find_window_node(&view, TEST_ORDERED_WID_A + 10) == &left, true);
    TEST_CHECK(view_find_window_node(&view, TEST_ORDERED_WID_B) == &right, true);
    TEST_CHECK(view.insertion_point == TEST_ORDERED_WID_A + 10, true);
    TEST_CHECK(left.zoom == &root, true);
    TEST_CHECK(root.ratio == 0.37f, true);
    TEST_CHECK(left.window_count == 1, true);
});

TEST_FUNC(native_tab_preserves_stack_order,
{
    struct window_node node = {0};
    struct view view = {0};
    node.window_count = 3;
    for (int i = 0; i < 3; ++i) {
        node.window_list[i] = TEST_ORDERED_WID_A + i;
        node.window_order[i] = TEST_ORDERED_WID_A + 2 - i;
    }
    view.root = &node;
    view.layout = VIEW_STACK;

    bool swapped = view_swap_node_window(&view, TEST_ORDERED_WID_A, TEST_ORDERED_WID_A + 10);
    TEST_CHECK(swapped, true);
    TEST_CHECK(node.window_count, 3);
    TEST_CHECK(node.window_list[0] == TEST_ORDERED_WID_A + 10, true);
    TEST_CHECK(node.window_order[2] == TEST_ORDERED_WID_A + 10, true);
    TEST_CHECK(node.window_order[0] == TEST_ORDERED_WID_A + 2, true);
});

TEST_FUNC(native_tab_rejects_missing_node,
{
    struct window_node node = {0};
    struct view view = {0};
    node.window_count = 1;
    node.window_list[0] = node.window_order[0] = TEST_ORDERED_WID_A;
    view.root = &node;
    view.layout = VIEW_BSP;
    bool swapped = view_swap_node_window(&view, TEST_ORDERED_WID_B, TEST_ORDERED_WID_B + 10);
    TEST_CHECK(swapped, false);
    TEST_CHECK(node.window_list[0] == TEST_ORDERED_WID_A, true);
});

TEST_FUNC(native_tab_release_preserves_ordered_out,
{
    struct test_ordered t;
    test_ordered_init(&t);
    window_set_flag(&t.a, WINDOW_TAB_MEMBER);
    window_set_flag(&t.a, WINDOW_ORDERED_OUT);
    window_manager_release_tab_members(&g_window_manager);
    TEST_CHECK(window_check_flag(&t.a, WINDOW_TAB_MEMBER), false);
    TEST_CHECK(window_check_flag(&t.a, WINDOW_ORDERED_OUT), true);
    test_ordered_free(&t);
});

TEST_FUNC(native_tab_hidden_event_keeps_handover_node,
{
    struct test_ordered t;
    test_ordered_init(&t);
    bool saved = g_window_manager.window_native_tabs;
    g_window_manager.window_native_tabs = true;
    window_set_flag(&t.a, WINDOW_TAB_MEMBER);

    // Simulate the delayed hide notification after tab membership was recognized.
    EVENT_HANDLER_SLS_WINDOW_IS_INVISIBLE((void *)(intptr_t) t.a.id, 1);
    TEST_CHECK(window_check_flag(&t.a, WINDOW_ORDERED_OUT), false);
    TEST_CHECK(view_find_window_node(t.view, t.a.id) != NULL, true);
    g_window_manager.window_native_tabs = saved;
    test_ordered_free(&t);
});

TEST_FUNC(native_tab_order_in_does_not_tile_hidden_tab,
{
    struct test_ordered t;
    test_ordered_init(&t);
    window_manager_window_did_order_out(&g_window_manager, &t.a);
    window_set_flag(&t.a, WINDOW_TAB_MEMBER);
    window_manager_window_did_order_in(&g_window_manager, &t.a, TEST_ORDERED_SID);
    TEST_CHECK(window_check_flag(&t.a, WINDOW_ORDERED_OUT), false);
    TEST_CHECK(window_check_flag(&t.a, WINDOW_TAB_MEMBER), true);
    TEST_CHECK(view_find_window_node(t.view, t.a.id) == NULL, true);
    test_ordered_free(&t);
});

TEST_FUNC(native_tabs_keep_ordinary_hide_on_close,
{
    struct test_ordered t;
    test_ordered_init(&t);
    bool saved = g_window_manager.window_native_tabs;
    g_window_manager.window_native_tabs = true;
    EVENT_HANDLER_SLS_WINDOW_IS_INVISIBLE((void *)(intptr_t) t.a.id, 1);
    TEST_CHECK(window_check_flag(&t.a, WINDOW_ORDERED_OUT), true);
    TEST_CHECK(view_find_window_node(t.view, t.a.id) == NULL, true);
    TEST_CHECK(view_find_window_node(t.view, t.b.id) != NULL, true);
    g_window_manager.window_native_tabs = saved;
    test_ordered_free(&t);
});

TEST_FUNC(native_tab_removal_ring_deduplicates,
{
    memset(&g_tab_ordered_out, 0, sizeof(g_tab_ordered_out));
    tab_ordered_out_record(TEST_ORDERED_WID_A);
    for (int i = 0; i < TAB_ORDERED_OUT_SLOTS * 2; ++i) {
        tab_ordered_out_record(TEST_ORDERED_WID_B);
    }
    TEST_CHECK(g_tab_ordered_out.wid[0] == TEST_ORDERED_WID_A, true);
    TEST_CHECK(g_tab_ordered_out.index, 2);
    memset(&g_tab_ordered_out, 0, sizeof(g_tab_ordered_out));
});

TEST_FUNC(native_tab_float_handover_preserves_focus,
{
    struct test_ordered t;
    test_ordered_init(&t);
    uint32_t saved = g_window_manager.focused_window_id;
    g_window_manager.focused_window_id = t.a.id;
    window_set_flag(&t.a, WINDOW_FLOAT);
    window_set_flag(&t.b, WINDOW_TAB_MEMBER);
    tabbed_window_hand_over_float(&t.a, &t.b);
    TEST_CHECK(window_check_flag(&t.a, WINDOW_FLOAT), false);
    TEST_CHECK(window_check_flag(&t.a, WINDOW_TAB_MEMBER), true);
    TEST_CHECK(window_check_flag(&t.b, WINDOW_FLOAT), true);
    TEST_CHECK(window_check_flag(&t.b, WINDOW_TAB_MEMBER), false);
    TEST_CHECK(g_window_manager.focused_window_id == t.b.id, true);
    g_window_manager.focused_window_id = saved;
    test_ordered_free(&t);
});
