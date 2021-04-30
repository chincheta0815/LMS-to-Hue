/*
 *  Squeeze2raop - LMS to Hue gateway
 *
 *  (c) Rouven
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdarg.h>

#include "conf_util.h"
#include "log_util.h"

/*----------------------------------------------------------------------------*/
/* globals */
/*----------------------------------------------------------------------------*/
extern log_level    main_loglevel;
extern log_level    huebridge_loglevel;

/*----------------------------------------------------------------------------*/
/* locals */
/*----------------------------------------------------------------------------*/
extern log_level    util_loglevel;
static log_level    *loglevel = &util_loglevel;


void MakeMacUnique(struct sMR *Device) {
    int i;

    for (i = 0; i < MAX_RENDERERS; i++) {
        if (!glMRDevices[i].Running || Device == &glMRDevices[i] )
            continue;

        if (!memcmp(&glMRDevices[i].sq_config.mac, &Device->sq_config.mac, 6)) {
            u32_t hash = hash32(Device->UDN);

            LOG_INFO("[%p]: found non-unique client mac address and correcting ...", Device);
            memset(&Device->sq_config.mac[0], 0xaa, 2);
            memcpy(&Device->sq_config.mac[0] + 2, &hash, 4);
        }
    }
}

/*----------------------------------------------------------------------------*/
void SaveConfig(char *name, void *ref, int mode) {
    struct sMR *p;
    IXML_Document *doc = ixmlDocument_createDocument();
    IXML_Document *old_doc = ref;
    IXML_Node *root, *common;
    IXML_NodeList *list;
    IXML_Element *old_root;
    char *s;
    FILE *file;
    int i;
    bool force = (mode == CONFIG_MIGRATE);

    old_root = ixmlDocument_getElementById(old_doc, "squeeze2hue");

    if (mode != CONFIG_CREATE && old_doc) {
        ixmlDocument_importNode(doc, (IXML_Node*) old_root, true, &root);
        ixmlNode_appendChild((IXML_Node*) doc, root);

        list = ixmlDocument_getElementsByTagName((IXML_Document*) root, "device");
        for (i = 0; i < (int) ixmlNodeList_length(list); i++) {
            IXML_Node *device;

            device = ixmlNodeList_item(list, i);
            ixmlNode_removeChild(root, device, &device);
            ixmlNode_free(device);
        }
        if (list)
            ixmlNodeList_free(list);
        common = (IXML_Node*) ixmlDocument_getElementById((IXML_Document*) root, "common");
    }
    else {
        root = XMLAddNode(doc, NULL, "squeeze2hue", NULL);
        common = (IXML_Node*) XMLAddNode(doc, root, "common", NULL);
    }

    XMLUpdateNode(doc, root, force, "interface", glInterface);

    XMLUpdateNode(doc, root, force, "main_log",level2debug(main_loglevel));
    XMLUpdateNode(doc, root, force, "util_log",level2debug(util_loglevel));
    XMLUpdateNode(doc, root, force, "huebridge_log", level2debug(huebridge_loglevel));

    XMLUpdateNode(doc, root, force, "scan_interval", "%d", (u32_t) glScanInterval);
    XMLUpdateNode(doc, root, force, "scan_timeout", "%d", (u32_t) glScanTimeout);
    XMLUpdateNode(doc, root, force, "log_limit", "%d", (s32_t) glLogLimit);

    XMLUpdateNode(doc, common, force, "streambuf_size", "%d", (u32_t) glDeviceParam.streambuf_size);
    XMLUpdateNode(doc, common, force, "output_size", "%d", (u32_t) glDeviceParam.outputbuf_size);
    XMLUpdateNode(doc, common, force, "enabled", "%d", (int) glMRConfig.Enabled);
    XMLUpdateNode(doc, common, force, "codecs", glDeviceParam.codecs);
    XMLUpdateNode(doc, common, force, "sample_rate", "%d", (int) glDeviceParam.sample_rate);
#if defined(RESAMPLE)
    XMLUpdateNode(doc, common, force, "resample", "%d", (int) glDeviceParam.resample);
    XMLUpdateNode(doc, common, force, "resample_options", glDeviceParam.resample_options);
#endif
    XMLUpdateNode(doc, common, force, "auto_play", "%d", (int) glMRConfig.AutoPlay);
    XMLUpdateNode(doc, common, force, "remove_timeout", "%d", (int) glMRConfig.RemoveTimeout);
    XMLUpdateNode(doc, common, force, "server", glDeviceParam.server);

    // correct some buggy parameters
    if (glDeviceParam.sample_rate < 44100)
        XMLUpdateNode(doc, common, true, "sample_rate", "%d", 96000);

    for (i = 0; i < MAX_RENDERERS; i++) {
        IXML_Node *dev_node;

        if (!glMRDevices[i].Running)
            continue;
        else 
            p = &glMRDevices[i];

        // existing device, keep param and update "name" if LMS has requested it
        if (old_doc && ((dev_node = (IXML_Node*) FindMRConfig(old_doc, p->UDN)) != NULL)) {
            ixmlDocument_importNode(doc, dev_node, true, &dev_node);
            ixmlNode_appendChild((IXML_Node*) root, dev_node);

            XMLUpdateNode(doc, dev_node, true, "name", p->sq_config.name);
            XMLUpdateNode(doc, dev_node, true, "friendly_name", p->FriendlyName);
            XMLUpdateNode(doc, dev_node, true, "user_name", p->Config.UserName);
            XMLUpdateNode(doc, dev_node, true, "client_key", p->Config.ClientKey);

            if (memcmp(p->sq_config.mac, "\0\0\0\0\0\0", 6)) {
                XMLUpdateNode(doc, dev_node, true, "mac", "%02x:%02x:%02x:%02x:%02x:%02x", p->sq_config.mac[0],
                                p->sq_config.mac[1], p->sq_config.mac[2], p->sq_config.mac[3], p->sq_config.mac[4], p->sq_config.mac[5]);
            }

            if (*p->sq_config.dynamic.server) XMLUpdateNode(doc, dev_node, true, "server", p->sq_config.dynamic.server);
        }
        // new device, add nodes
        else {
             dev_node = XMLAddNode(doc, root, "device", NULL);
             XMLAddNode(doc, dev_node, "enabled", "%d", (int) p->Config.Enabled);
             XMLAddNode(doc, dev_node, "udn", p->UDN);
             XMLAddNode(doc, dev_node, "friendly_name", p->FriendlyName);
             XMLAddNode(doc, dev_node, "user_name", p->Config.UserName);
             XMLAddNode(doc, dev_node, "client_key", p->Config.ClientKey);
             if (*p->sq_config.dynamic.server)
                 XMLAddNode(doc, dev_node, "server", p->sq_config.dynamic.server);
             if (!memcmp(p->sq_config.mac, "\0\0\0\0\0\0", 6)) {
                 XMLAddNode(doc, dev_node, "mac", "%02x:%02x:%02x:%02x:%02x:%02x", p->sq_config.mac[0],
                              p->sq_config.mac[1], p->sq_config.mac[2], p->sq_config.mac[3], p->sq_config.mac[4], p->sq_config.mac[5]);
             }
        }
    }

    // add devices in old XML file that has not been discovered
    list = ixmlDocument_getElementsByTagName((IXML_Document*) old_root, "device");
    for (i = 0; i < (int) ixmlNodeList_length(list); i++) {
        char *udn;
        IXML_Node *device, *node;

        device = ixmlNodeList_item(list, i);
        node = (IXML_Node*) ixmlDocument_getElementById((IXML_Document*) device, "udn");
        node = ixmlNode_getFirstChild(node);
        udn = (char*) ixmlNode_getNodeValue(node);
        if (!FindMRConfig(doc, udn)) {
            ixmlDocument_importNode(doc, (IXML_Node*) device, true, &device);
            ixmlNode_appendChild((IXML_Node*) root, device);
        }
    }
    if (list)
        ixmlNodeList_free(list);

    file = fopen(name, "wb");
    s = ixmlDocumenttoString(doc);
    fwrite(s, 1, strlen(s), file);
    fclose(file);
    free(s);

    ixmlDocument_free(doc);
}


/*----------------------------------------------------------------------------*/
static void LoadConfigItem(tMRConfig *Conf, sq_dev_param_t *sq_conf, char *name, char *val) {
    if (!val)
        return;

    if (!strcmp(name, "streambuf_size"))
        sq_conf->streambuf_size = atol(val);
    if (!strcmp(name, "output_size"))
        sq_conf->outputbuf_size = atol(val);
    if (!strcmp(name, "codecs"))
        strcpy(sq_conf->codecs, val);
    if (!strcmp(name, "sample_rate")) 
        sq_conf->sample_rate = atol(val);
    if (!strcmp(name, "name")) 
        strcpy(sq_conf->name, val);
    if (!strcmp(name, "server"))
        strcpy(sq_conf->server, val);
    if (!strcmp(name, "mac"))  {
        unsigned mac[6];
        int i;
        // seems to be a Windows scanf buf, cannot support %hhx
        sscanf(val,"%2x:%2x:%2x:%2x:%2x:%2x", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
            for (i = 0; i < 6; i++)
                sq_conf->mac[i] = mac[i];
    }
#if defined(RESAMPLE)
    if (!strcmp(name, "resample"))
        sq_conf->resample = atol(val);
    if (!strcmp(name, "resample_options"))
        strcpy(sq_conf->resample_options, val);
#endif
    if (!strcmp(name, "enabled"))
        Conf->Enabled = atol(val);
    if (!strcmp(name, "auto_play"))
        Conf->AutoPlay = atol(val);
    if (!strcmp(name, "remove_timeout"))
        Conf->RemoveTimeout = atol(val);
    if (!strcmp(name, "user_name"))
        strcpy(Conf->UserName, val);
    if (!strcmp(name, "client_key"))
        strcpy(Conf->ClientKey, val);
}

/*----------------------------------------------------------------------------*/
static void LoadGlobalItem(char *name, char *val) {
    if (!val)
        return;

    // if (!strcmp(name, "server"))
    //    strcpy(glSQServer, val);
    // temporary to ensure parameter transfer from global to common
    if (!strcmp(name, "server"))
        strcpy(glDeviceParam.server, val);

    if (!strcmp(name, "interface"))
        strcpy(glInterface, val);

    if (!strcmp(name, "main_log"))
        main_loglevel = debug2level(val);
    if (!strcmp(name, "util_log")) 
        util_loglevel = debug2level(val);
    if (!strcmp(name, "huebridge_log"))
        huebridge_loglevel = debug2level(val);

    if (!strcmp(name, "scan_interval"))
        glScanInterval = atol(val);
    if (!strcmp(name, "scan_timeout"))
        glScanTimeout = atol(val);
    if (!strcmp(name, "log_limit"))
        glLogLimit = atol(val);
 }


/*----------------------------------------------------------------------------*/
void *FindMRConfig(void *ref, char *UDN) {
    IXML_Element *elm;
    IXML_Node *device = NULL;
    IXML_NodeList *l1_node_list;
    IXML_Document *doc = (IXML_Document*) ref;
    char *v;
    unsigned i;

    elm = ixmlDocument_getElementById(doc, "squeeze2hue");
    l1_node_list = ixmlDocument_getElementsByTagName((IXML_Document*) elm, "udn");
    for (i = 0; i < ixmlNodeList_length(l1_node_list); i++) {
        IXML_Node *l1_node, *l1_1_node;
        l1_node = ixmlNodeList_item(l1_node_list, i);
        l1_1_node = ixmlNode_getFirstChild(l1_node);
        v = (char*) ixmlNode_getNodeValue(l1_1_node);
        if (v && !strcmp(v, UDN)) {
            device = ixmlNode_getParentNode(l1_node);
            break;
        }
    }
    if (l1_node_list)
        ixmlNodeList_free(l1_node_list);
    return device;
}

/*----------------------------------------------------------------------------*/
void *LoadMRConfig(void *ref, char *UDN, tMRConfig *Conf, sq_dev_param_t *sq_conf) {
    IXML_NodeList *node_list;
    IXML_Document *doc = (IXML_Document*) ref;
    IXML_Node *node;
    char *n, *v;
    unsigned i;

    node = (IXML_Node*) FindMRConfig(doc, UDN);
    if (node) {
        node_list = ixmlNode_getChildNodes(node);
        for (i = 0; i < ixmlNodeList_length(node_list); i++) {
            IXML_Node *l1_node, *l1_1_node;
            l1_node = ixmlNodeList_item(node_list, i);
            n = (char*) ixmlNode_getNodeName(l1_node);
            l1_1_node = ixmlNode_getFirstChild(l1_node);
            v = (char*) ixmlNode_getNodeValue(l1_1_node);
            LoadConfigItem(Conf, sq_conf, n, v);
        }
        if (node_list)
            ixmlNodeList_free(node_list);
    }

    return node;
}

/*----------------------------------------------------------------------------*/
void *LoadConfig(char *name, tMRConfig *Conf, sq_dev_param_t *sq_conf) {
    IXML_Element *elm;
    IXML_Document *doc;

    doc = ixmlLoadDocument(name);
    if (!doc)
        return NULL;

    elm = ixmlDocument_getElementById(doc, "squeeze2hue");
    if (elm) {
        unsigned i;
        char *n, *v;
        IXML_NodeList *l1_node_list;
            l1_node_list = ixmlNode_getChildNodes((IXML_Node*) elm);
            for (i = 0; i < ixmlNodeList_length(l1_node_list); i++) {
                IXML_Node *l1_node, *l1_1_node;
                l1_node = ixmlNodeList_item(l1_node_list, i);
                n = (char*) ixmlNode_getNodeName(l1_node);
                l1_1_node = ixmlNode_getFirstChild(l1_node);
                v = (char*) ixmlNode_getNodeValue(l1_1_node);
                    LoadGlobalItem(n, v);
            }
            if (l1_node_list)
                ixmlNodeList_free(l1_node_list);
    }

    elm = ixmlDocument_getElementById((IXML_Document	*)elm, "common");
    if (elm) {
        char *n, *v;
        IXML_NodeList *l1_node_list;
        unsigned i;
        l1_node_list = ixmlNode_getChildNodes((IXML_Node*) elm);
        for (i = 0; i < ixmlNodeList_length(l1_node_list); i++) {
            IXML_Node *l1_node, *l1_1_node;
            l1_node = ixmlNodeList_item(l1_node_list, i);
            n = (char*) ixmlNode_getNodeName(l1_node);
            l1_1_node = ixmlNode_getFirstChild(l1_node);
            v = (char*) ixmlNode_getNodeValue(l1_1_node);
            LoadConfigItem(&glMRConfig, &glDeviceParam, n, v);
        }
            if (l1_node_list)
                ixmlNodeList_free(l1_node_list);
    }

    return doc;
}



