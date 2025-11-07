import grpc
import sys
# Bu dosyaları derlemek için: python -m grpc_tools.protoc -I./proto --python_out=. --grpc_python_out=. ./proto/local.proto
import local_pb2
import local_pb2_grpc

def run(prompt: str, server_address: str = 'localhost:16061'):
    """gRPC sunucusuna bağlanır ve bir prompt için token akışı başlatır."""
    print(f"🔌 Sunucuya bağlanılıyor: {server_address}")

    try:
        with grpc.insecure_channel(server_address) as channel:
            stub = local_pb2_grpc.LLMLocalServiceStub(channel)

            print(f"💬 Gönderilen Prompt: '{prompt}'")
            print("--- AI Yanıtı ---")
            
            # Yeni parametreler eklendi:
            request = local_pb2.LocalGenerateStreamRequest(
                prompt=prompt, 
                temperature=0.75, # Varsayılan ayardan biraz daha sıcak
                top_k=50,
                top_p=0.9
            )
            
            full_response = ""
            for response in stub.LocalGenerateStream(request):
                token = response.token
                print(token, end='', flush=True)
                full_response += token

            print("\n-------------------")
            print("✅ Akış başarıyla tamamlandı.")

    except grpc.RpcError as e:
        print(f"\n❌ HATA: gRPC çağrısı başarısız oldu.")
        print(f"   - Durum: {e.code()}")
        print(f"   - Detaylar: {e.details()}")
    except Exception as e:
        print(f"\n❌ Beklenmedik bir hata oluştu: {e}")

if __name__ == '__main__':
    # ... (Aynı kalır)
    if len(sys.argv) > 1:
        user_prompt = " ".join(sys.argv[1:])
    else:
        user_prompt = "Türkiye'nin başkenti neresidir ve bu şehir hakkında kısa bir bilgi ver."

    run(user_prompt)