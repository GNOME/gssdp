// Work-around gnome bug 670673; remove once fixed
namespace GSSDP {
[CCode (cheader_filename = "libgssdp/gssdp.h", cprefix = "GSSDP_ERROR_")]
	public errordomain Error {
		NO_IP_ADDRESS,
		FAILED;
		public static GLib.Quark quark ();
	}
}
