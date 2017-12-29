/**
 * A loadable plugin for Wireshark that can dissect the lichtenstein protocol
 * on-the-wire and display information about it.
 *
 * Build using `clang wireshark_dissector.cpp -lstdc++ -shared -undefined dynamic_lookup -I"/usr/local/include/glib-2.0/" -I"/Users/tristan/Programming/wireshark/" -o wireshark_dissector.so`
 *
 * This code is honestly a piece of shit and _really_ needs to be refactored,
 * but it was written pretty quckly and works, so meh.
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../src/crc32/crc32.cpp"
#include "../src/lichtenstein_proto.h"

// include wireshark headers
#include <epan/packet.h>

#define LICHTENSTEIN_PORT 7420

static int hf_lichtenstein_flag_multicast = -1;
static int hf_lichtenstein_flag_response = -1;
static int hf_lichtenstein_flag_ack = -1;
static int hf_lichtenstein_flag_nack = -1;
static int hf_lichtenstein_flag_checksummed = -1;

static int proto_lichtenstein = -1;

static gint ett_lichtenstein = -1;
static gint ett_lichtenstein_flags = -1;

// global header symbols
static int hf_lichtenstein_magic = -1;
static int hf_lichtenstein_version = -1;
static int hf_lichtenstein_crc = -1;

static int hf_lichtenstein_opcode = -1;
static int hf_lichtenstein_flags = -1;

static int hf_lichtenstein_seq_idx = -1;
static int hf_lichtenstein_seq_len = -1;

static int hf_lichtenstein_txn = -1;
static int hf_lichtenstein_payloadLen = -1;

// strings
static const value_string opcodes[] = {
	{ 0, "Node Announcement" },
	{ 1, "Server Announcement" },
	{ 2, "Node Adoption" },
	{ 3, "Node Status" },
	{ 4, "Framebuffer Data" },
	{ 5, "Config" },
	{ 6, "Sync Output" },
	{ 7, "Read GPIO" },
	{ 8, "Write GPIO" },
	{ 9, "System Reset" },
	{ 10, "System Sleep" },
	{ 11, "Keepalive" },
	{ 12, "NVRAM Reconfig" },
	{ 0, NULL }
};

// version symbol
__attribute__ ((visibility("default"))) extern "C" char version[] = "1.0";

/**
 * this function actually dissects the protocol
 */
static int dissect_lichtenstein(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree _U_, void *data _U_) {
	// set info column
	col_clear(pinfo->cinfo,COL_INFO);

    gint offset = 0;
    guint16 packet_type = tvb_get_guint16(tvb, 12, ENC_BIG_ENDIAN);

    col_set_str(pinfo->cinfo, COL_PROTOCOL, "LICT");
    col_clear(pinfo->cinfo,COL_INFO);
    col_add_fstr(pinfo->cinfo, COL_INFO, "Opcode: %s",
             val_to_str(packet_type, opcodes, "Unknown (0x%04x)"));

	// root node
	proto_item *ti = proto_tree_add_item(tree, proto_lichtenstein, tvb, 0, -1, ENC_NA);
    proto_item_append_text(ti, ", %s",
        val_to_str(packet_type, opcodes, "Unknown (0x%04x)"));

	// add elements to the protocol header subtree
    proto_tree *header = proto_item_add_subtree(ti, ett_lichtenstein);

	proto_tree_add_item(header, hf_lichtenstein_magic, tvb, 0, 4, ENC_BIG_ENDIAN);
	proto_tree_add_item(header, hf_lichtenstein_version, tvb, 4, 4, ENC_BIG_ENDIAN);
    proto_tree_add_item(header, hf_lichtenstein_crc, tvb, 8, 4, ENC_BIG_ENDIAN);

	proto_tree_add_item(header, hf_lichtenstein_opcode, tvb, 12, 2, ENC_BIG_ENDIAN);

	proto_tree_add_item(header, hf_lichtenstein_flags, tvb, 14, 2, ENC_BIG_ENDIAN);
	proto_tree_add_item(header, hf_lichtenstein_flag_multicast, tvb, 14, 2, ENC_BIG_ENDIAN);
	proto_tree_add_item(header, hf_lichtenstein_flag_response, tvb, 14, 2, ENC_BIG_ENDIAN);
	proto_tree_add_item(header, hf_lichtenstein_flag_ack, tvb, 14, 2, ENC_BIG_ENDIAN);
	proto_tree_add_item(header, hf_lichtenstein_flag_nack, tvb, 14, 2, ENC_BIG_ENDIAN);
    proto_tree_add_item(header, hf_lichtenstein_flag_checksummed, tvb, 14, 2, ENC_BIG_ENDIAN);

	proto_tree_add_item(header, hf_lichtenstein_seq_idx, tvb, 16, 2, ENC_BIG_ENDIAN);
	proto_tree_add_item(header, hf_lichtenstein_seq_len, tvb, 18, 2, ENC_BIG_ENDIAN);

	proto_tree_add_item(header, hf_lichtenstein_txn, tvb, 20, 4, ENC_BIG_ENDIAN);

	proto_tree_add_item(header, hf_lichtenstein_payloadLen, tvb, 24, 4, ENC_BIG_ENDIAN);

	return tvb_captured_length(tvb);
}

/**
 * register the protocol with wireshark
 */
extern "C" void proto_register_lichtenstein(void) {
    static hf_register_info hf[] = {
		{ &hf_lichtenstein_magic,
            { "Magic", "lict.magic",
            FT_UINT32, BASE_HEX,
            NULL, 0x0,
            NULL, HFILL }
        }, { &hf_lichtenstein_version,
            { "Protocol Version", "lict.version",
            FT_UINT32, BASE_HEX,
            NULL, 0x0,
            NULL, HFILL }
        }, { &hf_lichtenstein_crc,
            { "CRC32", "lict.crc32",
            FT_UINT32, BASE_HEX,
            NULL, 0x0,
            NULL, HFILL }
        }, { &hf_lichtenstein_opcode,
            { "Opcode", "lict.opcode",
            FT_UINT16, BASE_HEX,
			VALS(opcodes), 0x0,
            NULL, HFILL }
        },

		{ &hf_lichtenstein_flags,
            { "Flags", "lict.flags",
            FT_UINT16, BASE_HEX,
            NULL, 0x0,
            NULL, HFILL }
        },
        { &hf_lichtenstein_flag_multicast,
            { "LICT Multicast Flag", "lict.flags.multicast",
            FT_BOOLEAN, 16,
            NULL, kFlagMulticast,
            NULL, HFILL }
        },
        { &hf_lichtenstein_flag_response,
            { "LICT Response Flag", "lict.flags.response",
            FT_BOOLEAN, 16,
            NULL, kFlagResponse,
            NULL, HFILL }
        },
        { &hf_lichtenstein_flag_ack,
            { "LICT Ack Flag", "lict.flags.ack",
            FT_BOOLEAN, 16,
            NULL, kFlagAck,
            NULL, HFILL }
        },
        { &hf_lichtenstein_flag_nack,
            { "LICT NAck Flag", "lict.flags.nack",
            FT_BOOLEAN, 16,
            NULL, kFlagNAck,
            NULL, HFILL }
        },
        { &hf_lichtenstein_flag_checksummed,
            { "LICT Checksum Present Flag", "lict.flags.checksum",
            FT_BOOLEAN, 16,
            NULL, kFlagChecksummed,
            NULL, HFILL }
        },

		{ &hf_lichtenstein_seq_idx,
            { "Sequence Index", "lict.seq.idx",
            FT_UINT16, BASE_DEC,
            NULL, 0x0,
            NULL, HFILL }
        }, { &hf_lichtenstein_seq_len,
            { "Sequence Length", "lict.seq.len",
            FT_UINT16, BASE_DEC,
            NULL, 0x0,
            NULL, HFILL }
        },

		{ &hf_lichtenstein_txn,
            { "Transaction Number", "lict.txn",
            FT_UINT32, BASE_DEC,
            NULL, 0x0,
            NULL, HFILL }
        },

		{ &hf_lichtenstein_payloadLen,
            { "Payload Length (bytes)", "lict.payload.len",
            FT_UINT32, BASE_DEC,
            NULL, 0x0,
            NULL, HFILL }
        },
    };

    /* Setup protocol subtree array */
    static gint *ett[] = {
        &ett_lichtenstein,
		&ett_lichtenstein_flags
    };

    proto_lichtenstein = proto_register_protocol (
        "Lichtenstein Protocol", /* name       */
        "Lichtenstein",      /* short name */
        "lict"       /* abbrev     */
        );

    proto_register_field_array(proto_lichtenstein, hf, array_length(hf));
    proto_register_subtree_array(ett, array_length(ett));
}

/**
 * set up the protocol and tell wireshark that we run on UDP port 7420
 */
extern "C" void proto_reg_handoff_lichtenstein(void) {
    static dissector_handle_t lichtenstein_handle;

    lichtenstein_handle = create_dissector_handle(dissect_lichtenstein, proto_lichtenstein);
    dissector_add_uint("udp.port", LICHTENSTEIN_PORT, lichtenstein_handle);
}

extern "C" void plugin_register(void) {
	// register protocol
	if(proto_lichtenstein == -1)  {
		proto_register_lichtenstein();
	}
}

extern "C" void plugin_reg_handoff(void) {
    proto_reg_handoff_lichtenstein();
}
