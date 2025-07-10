#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/smf.h>

LOG_MODULE_REGISTER(test, LOG_LEVEL_INF);

static void s1_entry(void* data);
static enum smf_state_result s1_run(void* data);
static void s1_exit(void* data);
static void s2_entry(void* data);
static enum smf_state_result s2_run(void* data);
static void s2_exit(void* data);

typedef struct {
    struct smf_ctx ctx;
    uint32_t state;
} test_state_t;

typedef enum {
    TEST_STATE_1,
    TEST_STATE_2,
    TEST_STATE_3,
} test_state_enum_t;

test_state_t test_state = {
    .ctx =
        {
              .current       = NULL,
              .previous      = NULL,
              .terminate_val = 0,
              .internal      = 0,
              },
    .state = 0,
};
static struct smf_state test_states[] = {
    [TEST_STATE_1] = SMF_CREATE_STATE(s1_entry, s1_run, s1_exit, NULL, NULL),
    [TEST_STATE_2] = SMF_CREATE_STATE(s2_entry, s2_run, s2_exit, NULL, NULL),
    // SMF_CREATE_STATE(NULL, NULL, NULL, NULL, NULL),
};

int main(void) {
    smf_set_initial(SMF_CTX(&test_state), &test_states[TEST_STATE_1]);
    while (1) {
        int ret = smf_run_state(SMF_CTX(&test_state));
        if (ret != 0) {
            LOG_ERR("State machine run failed with error: %d", ret);
            return ret;
        }
        k_msleep(1000); // Simulate some delay
        // Example state transition logic
    }
}

static void s1_entry(void* data) {
    test_state_t* state = (test_state_t*)data;
    if (state->state == TEST_STATE_1) {
        LOG_INF("Entering State 1");
    } else {
        LOG_ERR("State mismatch: expected %d, got %d", TEST_STATE_1, state->state);
        return;
    }
    printf("State 1 entry.\n");
}

static enum smf_state_result s1_run(void* data) {
    printf("State 1 run.\n");
    return SMF_EVENT_HANDLED; // or SMF_EVENT_PROPAGATE based on logic
}

static void s1_exit(void* data) {
    printf("State 1 exit.\n");
}

static void s2_entry(void* data) {
    printf("State 2 entry.\n");
}

static enum smf_state_result s2_run(void* data) {
    printf("State 2 run.\n");
    return SMF_EVENT_HANDLED; // or SMF_EVENT_PROPAGATE based on logic
}

static void s2_exit(void* data) {
    printf("State 2 exit.\n");
}