/*
 * Entitlements for the A04BR3 built-in AMFI interface (bsd/kern/a04br3_amfi.c).
 *
 * Apple's AMFI keeps a CoreEntitlements context per code signature; that
 * library is closed. Here each signature with an XML entitlements blob gets
 * a A04BR3Entitlements object: the blob parsed into an OSDictionary (the
 * representation IOKit and the kernel KPIs hand out) plus the original XML,
 * which csops(CS_OPS_ENTITLEMENTS_BLOB) returns. The blob was already
 * checked against the code directory by csblob_get_entitlements().
 */
#include <pexpert/arm64/board_config.h>

#if defined(A04BR3)
#include <IOKit/IOLib.h>
#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSBoolean.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSString.h>
#include <libkern/c++/OSUnserialize.h>
#include <kern/cs_blobs.h>

class A04BR3Entitlements : public OSObject {
	OSDeclareDefaultStructors(A04BR3Entitlements);

public:
	OSDictionary *dict;
	CS_GenericBlob *xml;            /* the signature's blob, header included */
	size_t xml_len;

	bool init(void) APPLE_KEXT_OVERRIDE;
	void free(void) APPLE_KEXT_OVERRIDE;
};

OSDefineMetaClassAndStructors(A04BR3Entitlements, OSObject);

bool
A04BR3Entitlements::init(void)
{
	return OSObject::init();
}

void
A04BR3Entitlements::free(void)
{
	OSSafeReleaseNULL(dict);
	if (xml) {
		kfree_data(xml, xml_len);
		xml = NULL;
	}
	OSObject::free();
}

static A04BR3Entitlements *
a04br3_ent(const void *obj)
{
	return OSDynamicCast(A04BR3Entitlements, (OSObject *)(uintptr_t)obj);
}

extern "C" {
/* Parsed entitlements for one XML blob (header included), or NULL. Retained. */
void *
a04br3_entitlements_create(const CS_GenericBlob *blob, size_t blob_len)
{
	if (blob == NULL || blob_len <= sizeof(CS_GenericBlob) ||
	    ntohl(blob->magic) != CSMAGIC_EMBEDDED_ENTITLEMENTS ||
	    ntohl(blob->length) != blob_len) {
		return NULL;
	}

	/* OSUnserializeXML needs a terminating NUL inside the buffer. */
	size_t text_len = blob_len - sizeof(CS_GenericBlob);
	char *text = (char *)kalloc_data(text_len + 1, Z_WAITOK);
	if (text == NULL) {
		return NULL;
	}
	memcpy(text, blob->data, text_len);
	text[text_len] = '\0';
	OSObject *parsed = OSUnserializeXML(text, text_len + 1);
	kfree_data(text, text_len + 1);

	OSDictionary *dict = OSDynamicCast(OSDictionary, parsed);
	if (dict == NULL) {
		OSSafeReleaseNULL(parsed);
		return NULL;
	}

	A04BR3Entitlements *ent = OSTypeAlloc(A04BR3Entitlements);
	CS_GenericBlob *xml = (CS_GenericBlob *)kalloc_data(blob_len, Z_WAITOK);
	if (ent == NULL || !ent->init() || xml == NULL) {
		OSSafeReleaseNULL(ent);
		if (xml) {
			kfree_data(xml, blob_len);
		}
		dict->release();
		return NULL;
	}
	memcpy(xml, blob, blob_len);
	ent->dict = dict;
	ent->xml = xml;
	ent->xml_len = blob_len;
	return ent;
}

/* The entitlements as an OSDictionary. Retained. */
void *
a04br3_entitlements_copy_dict(const void *obj)
{
	A04BR3Entitlements *ent = a04br3_ent(obj);
	if (ent == NULL) {
		return NULL;
	}
	ent->dict->retain();
	return ent->dict;
}

/* A kalloc_data copy of the XML blob; the caller frees it (blob length). */
bool
a04br3_entitlements_copy_xml(const void *obj, CS_GenericBlob **out)
{
	A04BR3Entitlements *ent = a04br3_ent(obj);
	if (ent == NULL) {
		return false;
	}
	CS_GenericBlob *copy = (CS_GenericBlob *)kalloc_data(ent->xml_len, Z_WAITOK);
	if (copy == NULL) {
		return false;
	}
	memcpy(copy, ent->xml, ent->xml_len);
	*out = copy;
	return true;
}

/* name is true (a boolean entitlement). */
bool
a04br3_entitlements_bool(const void *obj, const char *name)
{
	A04BR3Entitlements *ent = a04br3_ent(obj);
	return ent != NULL && ent->dict->getObject(name) == kOSBooleanTrue;
}

/* name is the string value, or an array containing it. */
bool
a04br3_entitlements_string(const void *obj, const char *name, const char *value)
{
	A04BR3Entitlements *ent = a04br3_ent(obj);
	if (ent == NULL || value == NULL) {
		return false;
	}
	OSObject *v = ent->dict->getObject(name);
	OSString *s = OSDynamicCast(OSString, v);
	if (s != NULL) {
		return s->isEqualTo(value);
	}
	OSArray *a = OSDynamicCast(OSArray, v);
	for (unsigned int i = 0; a != NULL && i < a->getCount(); i++) {
		s = OSDynamicCast(OSString, a->getObject(i));
		if (s != NULL && s->isEqualTo(value)) {
			return true;
		}
	}
	return false;
}

/* Number of entitlements. */
unsigned int
a04br3_entitlements_count(const void *obj)
{
	A04BR3Entitlements *ent = a04br3_ent(obj);
	return ent != NULL ? ent->dict->getCount() : 0;
}

/* The value of name, or NULL. Retained. */
void *
a04br3_entitlements_copy_object(const void *obj, const char *name)
{
	A04BR3Entitlements *ent = a04br3_ent(obj);
	OSObject *v = ent != NULL ? ent->dict->getObject(name) : NULL;
	if (v != NULL) {
		v->retain();
	}
	return v;
}
}
#endif /* A04BR3 */
