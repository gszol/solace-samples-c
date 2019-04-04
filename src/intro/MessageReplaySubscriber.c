/** @example ex/Intro/MessageReplaySubscriber.c
 */

/*
 *  Copyright 2012-2019 Solace Corporation. All rights reserved.
 *
 *  http://www.solace.com
 *
 *  This source is distributed under the terms and conditions
 *  of any contract or contracts between Solace and you or
 *  your company. If there are no contracts in place use of
 *  this source is not authorized. No support is provided and
 *  no distribution, sharing with others or re-use of this
 *  source is authorized unless specifically stated in the
 *  contracts referred to above.
 *
 *  MessageReplaySubscriber
 *
 *  This is a modification of the QueueSubscriber example for message replay.
 *
 *  First, when the flow connects, all messages are requested from the replay log.
 *  Comments show how to request the replay log from a specific start time.
 *
 *  Second, the flow event handler is extended for the possibility
 *  of a replay start event. This means the router initiated a replay,
 *  and the application must destroy and re-create the flow
 *  to receive the replayed messages.
 */

#include "os.h"
#include "solclient/solClient.h"
#include "solclient/solClientMsg.h"


// Data structure to hold everything
// the flow event callback needs.
// Basically the solClient_session_createFlow argument list.
struct callback_data_t {
    char **flowProps;
    solClient_opaqueSession_pt session_p;
    solClient_opaqueFlow_pt *flow_pp;
    solClient_flow_createFuncInfo_t *flowFuncInfo_p;
    size_t flowFuncInfo_size;
};

/* Message Count */
static int msgCount = 0;


/*****************************************************************************
 * sessionMessageReceiveCallback
 *
 * The message receive callback is mandatory for session creation.
 *****************************************************************************/
static          solClient_rxMsgCallback_returnCode_t
sessionMessageReceiveCallback ( solClient_opaqueSession_pt opaqueSession_p, solClient_opaqueMsg_pt msg_p, void *user_p )
{
    return SOLCLIENT_CALLBACK_OK;
}

/*****************************************************************************
 * sessionEventCallback
 *
 * The event callback function is mandatory for session creation.
 *****************************************************************************/
void
sessionEventCallback ( solClient_opaqueSession_pt opaqueSession_p,
                solClient_session_eventCallbackInfo_pt eventInfo_p, void *user_p )
{
}

/*****************************************************************************
 * flowEventCallback
 *
 * The event callback function is mandatory for flow creation.
 *****************************************************************************/
static void
flowEventCallback ( solClient_opaqueFlow_pt opaqueFlow_p, solClient_flow_eventCallbackInfo_pt eventInfo_p, void *user_p )
{
    struct callback_data_t *callback_data_p = (struct callback_data_t*) user_p;
    solClient_errorInfo_pt errorInfo_p;
    if (eventInfo_p->flowEvent == SOLCLIENT_FLOW_EVENT_DOWN_ERROR) {
        errorInfo_p = solClient_getLastErrorInfo();
        if (errorInfo_p->subCode == SOLCLIENT_SUBCODE_REPLAY_STARTED) {
            printf ( "Router indicating replay, reconnecting flow to recieve messages.\n" );
            // This way replayed messages can show up as many times as the replay restarts,
            // without the Assured Delivery subsystem filtering them as duplicates
            // the second and consecutive times.
            solClient_flow_destroy(callback_data_p->flow_pp);
            solClient_session_createFlow ( callback_data_p->flowProps,
                    callback_data_p->session_p,
                    callback_data_p->flow_pp,
                    callback_data_p->flowFuncInfo_p,
                    callback_data_p->flowFuncInfo_size);

            // Alternatively, the application could reconnect the session instead,
            // leaving the flow intact (relying on auto-rebind)
            // and not receive the messages from the replay log repeatedly:

            //solClient_opaqueSession_pt session_p = callback_data_p->session_p;
            //solClient_session_disconnect ( session_p );
            //solClient_session_connect ( session_p );

        }

    }
}

/*****************************************************************************
 * flowMessageReceiveCallback
 *
 * The message receive callback is mandatory for session creation.
 *****************************************************************************/
static          solClient_rxMsgCallback_returnCode_t
flowMessageReceiveCallback ( solClient_opaqueFlow_pt opaqueFlow_p, solClient_opaqueMsg_pt msg_p, void *user_p )
{
    solClient_msgId_t msgId;

    /* Process the message. */
    printf ( "Received message:\n" );
    solClient_msg_dump ( msg_p, NULL, 0 );
    printf ( "\n" );
    msgCount++;

    /* Acknowledge the message after processing it. */
    if ( solClient_msg_getMsgId ( msg_p, &msgId )  == SOLCLIENT_OK ) {
        printf ( "Acknowledging message Id: %lld.\n", msgId );
        solClient_flow_sendAck ( opaqueFlow_p, msgId );
    }

    return SOLCLIENT_CALLBACK_OK;
}

/*****************************************************************************
 * main
 *
 * The entry point to the application.
 *****************************************************************************/
int
main ( int argc, char *argv[] )
{
    /* Context */
    solClient_opaqueContext_pt context_p;
    solClient_context_createFuncInfo_t contextFuncInfo = SOLCLIENT_CONTEXT_CREATEFUNC_INITIALIZER;

    /* Session */
    solClient_opaqueSession_pt session_p;
    solClient_session_createFuncInfo_t sessionFuncInfo = SOLCLIENT_SESSION_CREATEFUNC_INITIALIZER;

    /* Flow */
    solClient_opaqueFlow_pt flow_p;
    solClient_flow_createFuncInfo_t flowFuncInfo = SOLCLIENT_FLOW_CREATEFUNC_INITIALIZER;

    /* Session Properties */
    const char     *sessionProps[20] = {0, };
    int             propIndex;

    /* Flow Properties */
    const char     *flowProps[20] = {0, };

    /* Provision Properties */
    const char     *provProps[20] = {0, };
    int             provIndex;

    /* Queue Network Name to be used with "solClient_session_endpointProvision()" */
    char            qNN[80];

    if ( argc < 6 ) {
        printf ( "Usage: MessageReplaySubscriber <msg_backbone_ip:port> <vpn> <client-username> <password> <queue>\n" );
        return -1;
    }

    /*************************************************************************
     * Initialize the API and setup logging level
     *************************************************************************/

    /* solClient needs to be initialized before any other API calls. */
    solClient_initialize ( SOLCLIENT_LOG_DEFAULT_FILTER, NULL );

    /*************************************************************************
     * CREATE A CONTEXT
     *************************************************************************/

    /*
     * Create a Context, and specify that the Context thread be created
     * automatically instead of having the application create its own
     * Context thread.
     */
    solClient_context_create ( SOLCLIENT_CONTEXT_PROPS_DEFAULT_WITH_CREATE_THREAD,
                                           &context_p, &contextFuncInfo, sizeof ( contextFuncInfo ) );

    /*************************************************************************
     * Create and connect a Session
     *************************************************************************/

    /* Configure the Session function information. */
    sessionFuncInfo.rxMsgInfo.callback_p = sessionMessageReceiveCallback;
    sessionFuncInfo.eventInfo.callback_p = sessionEventCallback;

    /* Configure the Session properties. */
    propIndex = 0;

    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_HOST;
    sessionProps[propIndex++] = argv[1];

    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_VPN_NAME;
    sessionProps[propIndex++] = argv[2];

    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_USERNAME;
    sessionProps[propIndex++] = argv[3];

    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_PASSWORD;
    sessionProps[propIndex++] = argv[4];

    /* Create the Session. */
    solClient_session_create ( ( char ** ) sessionProps,
                               context_p,
                               &session_p, &sessionFuncInfo, sizeof ( sessionFuncInfo ) );

    /* Connect the Session. */
    solClient_session_connect ( session_p );
    printf ( "Connected.\n" );

    /*************************************************************************
     * Provision a Queue
     *************************************************************************/

    /* Configure the Provision properties */
    provIndex = 0;

    provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_ID;
    provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_QUEUE;

    provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_NAME;
    provProps[provIndex++] = argv[5];

    provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_PERMISSION;
    provProps[provIndex++] = SOLCLIENT_ENDPOINT_PERM_DELETE;

    provProps[provIndex++] = SOLCLIENT_ENDPOINT_PROP_QUOTA_MB;
    provProps[provIndex++] = "100";

    /* Check if the endpoint provisioning is support */
    if ( !solClient_session_isCapable ( session_p, SOLCLIENT_SESSION_CAPABILITY_ENDPOINT_MANAGEMENT ) ) {

        printf ( "Endpoint management not supported on this appliance.\n" );
        return -1;
    }

    /* Try to provision the Queue. Ignore if already exists */
    solClient_session_endpointProvision ( ( char ** ) provProps,
                                          session_p,
                                          SOLCLIENT_PROVISION_FLAGS_WAITFORCONFIRM|
                                          SOLCLIENT_PROVISION_FLAGS_IGNORE_EXIST_ERRORS,
                                          NULL, qNN, sizeof ( qNN ) );

    /*************************************************************************
     * Create a Flow
     *************************************************************************/

    /* Configure the Flow function information */
    flowFuncInfo.rxMsgInfo.callback_p = flowMessageReceiveCallback;
    flowFuncInfo.eventInfo.callback_p = flowEventCallback;

    /* Configure the Flow properties */
    propIndex = 0;

    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_BLOCKING;
    flowProps[propIndex++] = SOLCLIENT_PROP_DISABLE_VAL;

    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_ID;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_QUEUE;

    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_ACKMODE;
    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_ACKMODE_CLIENT;

    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_NAME;
    flowProps[propIndex++] = argv[5];

    flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_REPLAY_START_LOCATION;
    flowProps[propIndex] = SOLCLIENT_FLOW_PROP_REPLAY_START_LOCATION_BEGINNING;

    // Alternative replay start specifications:
    //
    // Seconds since UNIX epoch:
    // flowProps[propIndex] = "DATE:1554331492";
    //
    // RFC3339 date without timezone:
    // flowProps[propIndex] = "DATE:2019-04-03T18:48:00Z";
    //
    // RFC3339 date with timezone:
    // flowProps[propIndex] = "DATE:2019-04-03T18:48:00Z-05:00";

    propIndex++;

    struct callback_data_t callback_data;
    callback_data.session_p = session_p;
    callback_data.flow_pp = &flow_p;
    callback_data.flowProps = ( char ** ) flowProps;
    callback_data.flowFuncInfo_p = &flowFuncInfo;
    callback_data.flowFuncInfo_size = sizeof(flowFuncInfo);

    flowFuncInfo.eventInfo.user_p = &callback_data;



    solClient_session_createFlow ( ( char ** ) flowProps,
                                   session_p,
                                   &flow_p, &flowFuncInfo, sizeof ( flowFuncInfo ) );

    /*************************************************************************
     * Wait for messages
     *************************************************************************/

    printf ( "Waiting for 10 messages......\n" );
    fflush ( stdout );
    while ( msgCount < 10 ) {
        SLEEP ( 1 );
    }

    printf ( "Exiting.\n" );

    /*************************************************************************
     * Cleanup
     *************************************************************************/

    /* Destroy the Flow */
    solClient_flow_destroy ( &flow_p );

    /* Disconnect the Session */
    solClient_session_disconnect ( session_p );

    /* Cleanup solClient. */
    solClient_cleanup (  );

    return 0;
}
